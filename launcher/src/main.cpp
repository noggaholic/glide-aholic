#include "glide/socket.h"
#include "resources.h"
#include "process.h"
#include "web.h"
#include "sessions.h"
#include "supervisor.h"
#include "glide/builds.h"
#include "glide/files.h"
#include "glide/injection.h"
#include "glide/slot_config.h"
#include <ws2tcpip.h>
#include <shellapi.h>
#include <cstdio>
#include <cwchar>

namespace {
std::atomic<bool> stopping{false};
BOOL WINAPI Control(DWORD event) {
    if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) {
        stopping = true;
        return TRUE;
    }
    return FALSE;
}
bool Number(const wchar_t* text, unsigned maximum, unsigned& output) {
    if (!text || !*text)
        return false;
    unsigned value = 0;
    for (; *text; ++text) {
        if (*text < L'0' || *text > L'9' || value > (maximum - static_cast<unsigned>(*text - L'0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(*text - L'0');
    }
    if (!value)
        return false;
    output = value;
    return true;
}
std::wstring Parent(const std::wstring& path) {
    return path.substr(0, path.find_last_of(L"/\\"));
}
bool Resolve(const std::wstring& name, std::string& address) {
    std::string host;
    if (!glide::WideToUtf8(name, host) || host.empty())
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
int Fail(const std::string& error) {
    std::fprintf(stderr, "Launcher: %s\n", error.c_str());
    return 1;
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    using namespace glide;
    if (argc == 2 && (!std::wcscmp(argv[1], L"--help") || !std::wcscmp(argv[1], L"-h"))) {
        std::puts("Usage: glide-launcher.exe <absolute-game-exe> [--windowed] [--server host:port] [--build N]");
        return 0;
    }
    if (argc < 2)
        return Fail("Expected an absolute game executable path. Use --help for usage.");
    unsigned overrideBuild = 0, port = 6112;
    bool windowed = false, serverSet = false;
    std::wstring host = L"127.0.0.1";
    for (int i = 2; i < argc; ++i) {
        if (!std::wcscmp(argv[i], L"--windowed") && !windowed)
            windowed = true;
        else if (!std::wcscmp(argv[i], L"--build") && !overrideBuild && i + 1 < argc) {
            if (!Number(argv[++i], UINT32_MAX, overrideBuild))
                return Fail("Invalid build number");
        } else if (!std::wcscmp(argv[i], L"--server") && !serverSet && i + 1 < argc) {
            const std::wstring value = argv[++i];
            const size_t colon = value.find_last_of(L':');
            if (colon == std::wstring::npos || !colon || !Number(value.c_str() + colon + 1, 65535, port))
                return Fail("Expected server host:port");
            host = value.substr(0, colon);
            serverSet = true;
        } else
            return Fail("Unknown, repeated or incomplete option");
    }
    const std::wstring input = argv[1];
    wchar_t absolute[32768];
    if (input.size() < 3 || input[1] != L':' || (input[2] != L'\\' && input[2] != L'/'))
        return Fail("Game executable path must be absolute");
    const DWORD length = GetFullPathNameW(input.c_str(), 32768, absolute, nullptr);
    if (!length || length >= 32768)
        return Fail("Cannot normalize game path");
    PeImage image;
    const BuildInfo* build = nullptr;
    if (!PeImage::FromFile(absolute, image) || DetectBuild(image, overrideBuild, build) != BuildDetection::Found)
        return Fail(
            "Executable SHA-256 has no verified RVA profile, or differs from --build. The client was not started.");
#ifndef _WIN64
    if (image.Machine() != Bitness::X86)
        return Fail("Use the 64-bit launcher for this game executable");
#endif
    wchar_t ownPath[32768];
    const DWORD ownLength = GetModuleFileNameW(nullptr, ownPath, 32768);
    if (!ownLength || ownLength >= 32768)
        return Fail("Cannot locate launcher files");
    const std::wstring binaryDirectory = Parent(ownPath), project = Parent(Parent(Parent(binaryDirectory)));
    const std::wstring buildText = std::to_wstring(build->build);
    const std::wstring content = project + L"\\data\\" + buildText + L"\\content";
    Network network;
    std::string address, error;
    if (build->build == 167409) {
        std::puts("China executable identified; connecting to the local server");
        std::fflush(stdout);
    }
    if (!network.Ready() || !Resolve(host, address))
        return Fail("Cannot resolve the selected server to IPv4");
    {
        Socket probe;
        if (!probe.Connect(address, static_cast<uint16_t>(port)))
            return Fail("The selected game server is not accepting connections");
    }
    if (build->build == 167409) {
        std::puts("China local server reachable; starting the launcher supervisor");
        std::fflush(stdout);
    }
    LaunchRequest inputRequest;
    inputRequest.executable = absolute;
    inputRequest.host = host;
    inputRequest.build = build->build;
    inputRequest.overrideBuild = overrideBuild;
    inputRequest.windowed = windowed;
    inputRequest.port = static_cast<uint16_t>(port);
    LaunchRequest request;
    if (!inputRequest.Normalize(request, error))
        return Fail(error);
    std::mutex startupMutex;
    // The supervisor worker below captures this mutex and waits for session initialization.
    // cppcheck-suppress localMutex
    std::unique_lock<std::mutex> startupLock(startupMutex);
    std::unique_ptr<LauncherSessions> sessions;
    SupervisorPipe supervisor;
    bool existing = false;
    if (!supervisor.Start(
            request,
            [&](const LaunchRequest& incoming) {
                std::lock_guard<std::mutex> guard(startupMutex);
                if (sessions)
                    return sessions->Launch(incoming);
                LaunchReply reply;
                reply.error = "Supervisor startup failed";
                return reply;
            },
            error, &existing)) {
        if (!existing)
            return Fail(error);
        LaunchReply reply;
        if (!SupervisorPipe::Forward(request, reply, 120000, error))
            return Fail(error);
        if (!reply.error.empty())
            return Fail(reply.error);
        std::printf("Client %u started by the existing supervisor (PID %u).\n", reply.slot, reply.pid);
        return 0;
    }
    LauncherWeb web;
    if (build->build == 167409) {
        std::puts("China supervisor ready; starting the local login page");
        std::fflush(stdout);
    }
    if (!web.Start(build->build, content, error)) {
        startupLock.unlock();
        supervisor.Stop();
        return Fail(error);
    }
    sessions.reset(new LauncherSessions(project, binaryDirectory, web.Port()));
    startupLock.unlock();
    SetConsoleCtrlHandler(Control, TRUE);
    if (build->build == 167409) {
        std::puts("China login service ready; preparing the private client");
        std::fflush(stdout);
    }
    const LaunchReply first = sessions->Launch(request);
    int result = first.error.empty() ? 0 : Fail(first.error);
    if (first.error.empty())
        std::printf("Client %u started (PID %u). Keep this launcher open; Ctrl+C closes its clients.\n", first.slot,
                    first.pid);
    while (!stopping) {
        bool empty = false;
        if (!sessions->Reap(empty, error)) {
            result = Fail(error);
            break;
        }
        if (empty)
            break;
        Sleep(100);
    }
    // Reject subsequent requests and finish any in-progress handler before releasing client resources.
    supervisor.Stop();
    sessions->Stop();
    web.Stop();
    return result;
}
