#include "server.h"
#include <Windows.h>
#include <cwchar>

namespace {
HANDLE stopEvent = nullptr;
BOOL WINAPI Stop(DWORD event) {
    if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT && event != CTRL_CLOSE_EVENT)
        return FALSE;
    if (stopEvent)
        SetEvent(stopEvent);
    return TRUE;
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    glide::ServerConfig config;
    std::wstring configPath, readyPath, stopName;
    for (int i = 1; i < argc; ++i) {
        if (std::wcscmp(argv[i], L"--help") == 0) {
            std::puts("Usage: glide-server [--config path] [--build number] [--verbose] [--ready-file path] "
                      "[--stop-event name]");
            return 0;
        }
        if (std::wcscmp(argv[i], L"--verbose") == 0) {
            config.verbose = true;
            continue;
        }
        if (std::wcscmp(argv[i], L"--ready-file") == 0 && i + 1 < argc && readyPath.empty()) {
            readyPath = argv[++i];
            continue;
        }
        if (std::wcscmp(argv[i], L"--stop-event") == 0 && i + 1 < argc && stopName.empty()) {
            stopName = argv[++i];
            continue;
        }
        if (std::wcscmp(argv[i], L"--config") == 0 && i + 1 < argc && configPath.empty()) {
            configPath = argv[++i];
            continue;
        }
        if (std::wcscmp(argv[i], L"--build") == 0 && i + 1 < argc && !config.build) {
            std::string text;
            if (glide::WideToUtf8(argv[++i], text) && glide::ParseUnsigned(text, UINT32_MAX, config.build) &&
                config.build)
                continue;
        }
        std::puts("Invalid arguments; use --help");
        return 2;
    }
    std::string error;
    if (!configPath.empty() && !glide::LoadServerConfig(configPath, config, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    glide::Network network;
    if (!network.Ready()) {
        std::puts("Cannot initialize Winsock");
        return 1;
    }
    stopEvent = stopName.empty() ? CreateEventW(nullptr, TRUE, FALSE, nullptr)
                                 : OpenEventW(SYNCHRONIZE | EVENT_MODIFY_STATE, FALSE, stopName.c_str());
    if (!stopEvent) {
        std::puts("Cannot create shutdown event");
        return 1;
    }
    if (!SetConsoleCtrlHandler(Stop, TRUE)) {
        CloseHandle(stopEvent);
        return 1;
    }
    glide::Server server(config);
    bool started = server.Start(error);
    if (started && !readyPath.empty()) {
        const std::string port = std::to_string(server.Port());
        if (!glide::WriteFileAtomic(readyPath, glide::ByteView(port.data(), port.size()), false)) {
            error = "Cannot create readiness file";
            started = false;
        }
    }
    if (started)
        server.Run(stopEvent);
    else
        std::fprintf(stderr, "%s\n", error.c_str());
    SetConsoleCtrlHandler(Stop, FALSE);
    CloseHandle(stopEvent);
    stopEvent = nullptr;
    return started && !server.Failed() ? 0 : 1;
}
