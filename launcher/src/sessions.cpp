#include "glide/socket.h"
#include "sessions.h"
#include "inject.h"
#include "glide/files.h"
#include "glide/injection.h"
#include "glide/slot_config.h"
#include <ws2tcpip.h>
#include <shellapi.h>
#include <cstdio>

namespace glide {
namespace {
bool ResolveServer(const std::wstring& name, std::string& address) {
    std::string host;
    if (!WideToUtf8(name, host) || host.empty())
        return false;
    addrinfo hints = {}, *list = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), nullptr, &hints, &list))
        return false;
    char text[INET_ADDRSTRLEN] = {};
    const bool ok =
        list && InetNtopA(AF_INET, &reinterpret_cast<sockaddr_in*>(list->ai_addr)->sin_addr, text, sizeof(text));
    if (ok)
        address = text;
    if (list)
        freeaddrinfo(list);
    return ok;
}
} // namespace
LauncherSessions::LauncherSessions(std::wstring project, std::wstring binaries, unsigned webPort)
    : project_(std::move(project)), binaries_(std::move(binaries)), webPort_(webPort) {}
LauncherSessions::Client::~Client() {
    // Originals and the private directory stay held until every member of the job has terminated.
    while (!process.Stop()) {
        std::fputs("Waiting for supervised processes to stop...\n", stderr);
        Sleep(100);
    }
}
LauncherSessions::~LauncherSessions() {
    Stop();
}
LaunchReply LauncherSessions::Launch(const LaunchRequest& input) {
    std::lock_guard<std::mutex> guard(mutex_);
    LaunchReply reply;
    if (closing_) {
        reply.error = "Supervisor is stopping";
        return reply;
    }
    LaunchRequest request;
    const BuildInfo* build = nullptr;
    if (!input.Normalize(request, reply.error) || !request.VerifyExecutable(build, reply.error))
        return reply;
#ifndef _WIN64
    if (build->bitness != Bitness::X86) {
        reply.error = "Use the 64-bit launcher for this client";
        return reply;
    }
#endif
    std::string address;
    if (!ResolveServer(request.host, address)) {
        reply.error = "Cannot resolve the selected server to IPv4";
        return reply;
    }
    {
        Socket probe;
        if (!probe.Connect(address, request.port)) {
            reply.error = "The selected game server is not accepting connections";
            return reply;
        }
    }
    const std::wstring buildText = std::to_wstring(build->build);
    const std::wstring manifest = project_ + L"\\builds\\" + buildText + L"\\" + build->launch.runtimeManifest;
    std::unique_ptr<Client> client(new Client);
    if (!PreparedRuntime::Prepare(request.executable, manifest, project_ + L"\\runtime", client->runtime, reply.error,
                                  project_ + L"\\data\\" + buildText + L"\\content"))
        return reply;
    SlotConfig config;
    config.build = build->build;
    config.slot = client->runtime->Slot().Number();
    if (!Utf8ToWide(address, config.serverHost)) {
        reply.error = "Cannot encode server address";
        return reply;
    }
    config.serverPort = request.port;
    config.webPort = webPort_;
    config.runtimeDirectory = client->runtime->Slot().Directory();
    config.dataDirectory = project_ + L"\\data\\" + buildText;
    config.logPath = config.runtimeDirectory + L"\\client.log";
    config.archivePath = client->runtime->Archive();
    config.manifestPath = manifest;
    config.sourceExecutable = request.executable;
    Environment inherited;
    std::vector<wchar_t> environment;
    if (!ReadEnvironment(inherited)) {
        reply.error = "Cannot read launcher environment";
        return reply;
    }
    if (!config.MakeEnvironment(inherited, environment, reply.error))
        return reply;
    int count = 0;
    const std::wstring extra = L"game " + std::wstring(build->launch.extraArguments);
    wchar_t** nativeArguments = CommandLineToArgvW(extra.c_str(), &count);
    if (!nativeArguments) {
        reply.error = "Cannot parse build launch arguments";
        return reply;
    }
    std::vector<std::wstring> arguments{L"-dat", client->runtime->Archive()};
    for (int i = 1; i < count; ++i)
        arguments.push_back(nativeArguments[i]);
    LocalFree(nativeArguments);
    if (request.windowed)
        arguments.push_back(L"-windowed");
    if (!client->process.StartSuspended(client->runtime->Executable(), arguments, config.runtimeDirectory, environment,
                                        reply.error, client->runtime->Slot().JobName()) ||
        !InjectClient(client->process.Process(), build->bitness, binaries_, build->launch.readyTimeoutMs,
                      reply.error) ||
        !client->process.Resume(reply.error))
        return reply;
    reply.pid = client->process.Pid();
    reply.slot = config.slot;
    clients_.push_back(std::move(client));
    return reply;
}
bool LauncherSessions::Reap(bool& empty, std::string& error) {
    std::lock_guard<std::mutex> guard(mutex_);
    for (auto it = clients_.begin(); it != clients_.end();) {
        unsigned count = 0;
        if (!(*it)->process.Active(count)) {
            error = "Cannot inspect supervised processes";
            return false;
        }
        if (!count) {
            DWORD code = 0;
            if (GetExitCodeProcess((*it)->process.Process(), &code))
                std::printf("Client %u ended (PID %lu, exit %lu / 0x%08lX).\n", (*it)->runtime->Slot().Number(),
                            (*it)->process.Pid(), code, code);
            it = clients_.erase(it);
        } else
            ++it;
    }
    empty = clients_.empty();
    if (empty)
        closing_ = true; // Serialize the final close with incoming launch requests.
    error.clear();
    return true;
}
void LauncherSessions::Stop() {
    std::lock_guard<std::mutex> guard(mutex_);
    closing_ = true;
    clients_.clear();
}
} // namespace glide
