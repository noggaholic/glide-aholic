#include "network_hooks.h"
#include "file_hooks.h"
#include "child_hooks.h"
#include "glide/slot_config.h"
#include "glide/runtime_manifest.h"
#include "glide/builds.h"
#include "glide/files.h"
#include "glide/injection.h"
#include "glide/log.h"
#include "runtime.h"
#include "profile.h"
#include <cwchar>

namespace glide {
namespace {
bool ModulePath(HMODULE module, std::wstring& out) {
    wchar_t path[32768];
    const DWORD size = GetModuleFileNameW(module, path, 32768);
    if (!size || size >= 32768)
        return false;
    out.assign(path, size);
    return true;
}
bool Start(ClientRuntime& runtime, HMODULE module, std::string& error) {
    Environment environment;
    error = "Cannot read client environment";
    if (!ReadEnvironment(environment) || !SlotConfig::Decode(environment, runtime.config, error))
        return false;
    const auto& config = runtime.config;
    // Separate logs allow several browser processes to initialize concurrently.
    if (!runtime.log.Open(config.logPath + L"." + std::to_wstring(GetCurrentProcessId()))) {
        error = "Cannot open client process log";
        return false;
    }
    Buffer bytes;
    error = "Cannot read runtime manifest";
    if (!ReadFileBytes(config.manifestPath, 1024 * 1024, bytes) || !bytes.Size())
        return false;
    if (!RuntimeManifest::Parse(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()),
                                runtime.manifest, error))
        return false;
    error = "Runtime manifest build differs from client configuration";
    if (runtime.manifest.build != config.build)
        return false;
    std::wstring imagePath, dllPath;
    error = "Cannot identify process or client DLL";
    if (!ModulePath(nullptr, imagePath) || !ModulePath(module, dllPath))
        return false;
    const std::wstring root = config.runtimeDirectory + L"\\";
    const bool game = !_wcsicmp(imagePath.c_str(), (root + runtime.manifest.executable).c_str());
    bool browser = false;
    ChildProtection children;
    children.log = &runtime.log;
    children.dllPath = dllPath;
    children.slot = config;
    for (const auto& child : runtime.manifest.children) {
        children.executables.push_back(root + child);
        if (!_wcsicmp(imagePath.c_str(), children.executables.back().c_str()))
            browser = true;
    }
    if (!game && !browser) {
        error = "Process is not listed in this private runtime manifest";
        return false;
    }
    PeImage image;
    if (game) {
        const BuildInfo* build = nullptr;
        if (!PeImage::FromModule(GetModuleHandleW(nullptr), image) ||
            DetectBuild(image, config.build, build) != BuildDetection::Found) {
            error = "Executable SHA-256 or loaded sections do not match the configured RVA profile";
            return false;
        }
        runtime.profile = ClientRegistry::Find(build->build);
        if (!runtime.profile || runtime.profile->Info().bitness != image.Machine()) {
            error = "No client profile is implemented for this game build";
            return false;
        }
    }
    children.readyTimeoutMs = BuildTable::Find(config.build)->launch.readyTimeoutMs;
    const size_t separator = config.sourceExecutable.find_last_of(L'\\');
    if (separator == std::wstring::npos) {
        error = "Cannot identify original resource directory";
        return false;
    }
    const std::wstring sourceRoot = config.sourceExecutable.substr(0, separator);
    if (_wcsicmp(config.archivePath.c_str(), (sourceRoot + L"\\" + runtime.manifest.archive).c_str())) {
        error = "Original archive differs from the runtime manifest";
        return false;
    }
    std::vector<std::wstring> originals{config.sourceExecutable, config.archivePath};
    for (const auto& resource : runtime.manifest.resources)
        originals.push_back(sourceRoot + L"\\" + resource);
    for (const auto& package : runtime.manifest.packages)
        originals.push_back(config.dataDirectory + L"\\content\\" + package.source);
    // Each game/browser retains its own locks, independently of the supervisor and its parent.
    if (!runtime.originals.Acquire(originals, error))
        return false;
    for (const auto& package : runtime.manifest.packages)
        if (!package.Verify(config.runtimeDirectory + L"\\" + package.destination, error))
            return false;
    std::string host, machine;
    wchar_t machineName[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD machineSize = MAX_COMPUTERNAME_LENGTH + 1;
    error = "Cannot configure client network protection";
    if (!WideToUtf8(config.serverHost, host) || !GetComputerNameW(machineName, &machineSize) ||
        !WideToUtf8(std::wstring(machineName, machineSize), machine))
        return false;
    NetworkPolicy network;
    if (!network.Configure(host, config.serverPort, config.webPort, machine))
        return false;
    FileProtection files;
    files.runtimeDirectory = config.runtimeDirectory;
    files.sourceArchive = config.archivePath;
    files.china = config.build == 167409;
    files.log = &runtime.log;
    if (!runtime.hooks.Initialize(error) || !InstallNetworkHooks(runtime.hooks, network, error, &runtime.log) ||
        !InstallFileHooks(runtime.hooks, files, error) || !InstallChildHooks(runtime.hooks, children, error))
        return false;
    if (game) {
        if (!runtime.profile->Prepare(runtime, image, GetModuleHandleW(nullptr), error))
            return false;
        for (const auto& hook : runtime.profile->CodeHooks())
            if (!runtime.hooks.Attach(image, GetModuleHandleW(nullptr), hook, error))
                return false;
    }
    if (!runtime.hooks.Enable(error))
        return false;
    if (game && !runtime.profile->AfterHooks(runtime, error))
        return false;
    runtime.log.Write("client", game ? "Client profile and process protection enabled"
                                     : "Browser network, file and child-process protection enabled");
    HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, ClientReadyEvent(GetCurrentProcessId()).c_str());
    if (!ready) {
        error = "Launcher readiness event is missing";
        return false;
    }
    const BOOL signaled = SetEvent(ready);
    CloseHandle(ready);
    if (!signaled) {
        error = "Cannot signal client readiness";
        return false;
    }
    return true;
}
DWORD WINAPI Initialize(void* parameter) {
    HMODULE module = static_cast<HMODULE>(parameter), pinned = nullptr;
    // No live unload: installed callbacks and thread-local state live until process exit.
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCWSTR>(module), &pinned)) {
        TerminateProcess(GetCurrentProcess(), ERROR_ACCESS_DISABLED_BY_POLICY);
        return 1;
    }
    auto* runtime = new ClientRuntime;
    std::string error;
    if (!Start(*runtime, module, error)) {
        runtime->log.Write("client", error);
        OutputDebugStringA(error.c_str());
        TerminateProcess(GetCurrentProcess(), ERROR_ACCESS_DISABLED_BY_POLICY);
        return 1;
    }
    return 0;
}
} // namespace
} // namespace glide
BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        HANDLE thread = CreateThread(nullptr, 0, glide::Initialize, module, 0, nullptr);
        if (!thread)
            return FALSE;
        CloseHandle(thread);
    }
    return TRUE;
}
