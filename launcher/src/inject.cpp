#include "inject.h"
#include "process.h"
#include "glide/injection.h"

namespace glide {
bool InjectVia32(HANDLE target, const std::wstring& helper, const std::wstring& dll, DWORD timeoutMs,
                 std::string& error) {
    const DWORD pid = GetProcessId(target);
    if (!pid || pid == GetCurrentProcessId()) {
        error = "Invalid suspended client";
        return false;
    }
    const auto fail = [&](const std::string& reason) {
        error = reason;
        TerminateProcess(target, ERROR_ACCESS_DISABLED_BY_POLICY);
        WaitForSingleObject(target, 5000);
        return false;
    };
    PeImage image, library;
    if (!timeoutMs || timeoutMs > 300000 || !PeImage::FromFile(helper.c_str(), image) ||
        image.Machine() != Bitness::X86 || !PeImage::FromFile(dll.c_str(), library) ||
        library.Machine() != Bitness::X86)
        return fail("The matching Win32 inject32.exe and client DLL are required");
    BOOL targetWow = FALSE;
    SYSTEM_INFO system = {};
    GetNativeSystemInfo(&system);
    if (!IsWow64Process(target, &targetWow) ||
        (system.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64 && !targetWow))
        return fail("inject32 requires a 32-bit client");
    ProcessTree helperProcess;
    if (!helperProcess.StartSuspended(helper, {std::to_wstring(pid), dll, std::to_wstring(timeoutMs)}, L"", {},
                                      error) ||
        !helperProcess.Resume(error))
        return fail(error);
    // Include startup/cleanup time, but never permit an unbounded helper wait.
    HANDLE wait[] = {helperProcess.Process(), target};
    if (WaitForMultipleObjects(2, wait, FALSE, timeoutMs + 10000) != WAIT_OBJECT_0)
        return fail("The client exited or the 32-bit injection helper timed out");
    DWORD code = 1;
    if (!GetExitCodeProcess(helperProcess.Process(), &code) || code != 0)
        return fail("32-bit injection failed (helper exit " + std::to_string(code) + ")");
    if (WaitForSingleObject(target, 0) != WAIT_TIMEOUT)
        return fail("The 32-bit client exited before it could be resumed");
    error.clear();
    return true;
}
bool InjectClient(HANDLE target, Bitness bitness, const std::wstring& binaries, DWORD timeoutMs, std::string& error) {
#ifdef _WIN64
    if (bitness == Bitness::X86) {
        const auto config = binaries.find_last_of(L"/\\");
        const auto platform = config == std::wstring::npos ? config : binaries.find_last_of(L"/\\", config - 1);
        if (platform == std::wstring::npos) {
            error = "Cannot locate Win32 build output";
            return false;
        }
        const auto win32 = binaries.substr(0, platform) + L"\\Win32" + binaries.substr(config);
        return InjectVia32(target, win32 + L"\\inject32.exe", win32 + L"\\glide-client.dll", timeoutMs, error);
    }
#else
    if (bitness != Bitness::X86) {
        error = "Use the 64-bit launcher for this client";
        return false;
    }
#endif
    return InjectAndWait(target, binaries + L"\\glide-client.dll", timeoutMs, error);
}
} // namespace glide
