#include "glide/injection.h"
#include "glide/pe.h"
#include <TlHelp32.h>
#include <cwchar>
#include <cstring>

namespace glide {
namespace {
class Handle {
  public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() {
        if (value_ && value_ != INVALID_HANDLE_VALUE)
            CloseHandle(value_);
    }
    HANDLE Get() const {
        return value_;
    }

  private:
    HANDLE value_;
};
LPTHREAD_START_ROUTINE RemoteLoader(DWORD pid) {
    const auto loader = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HMODULE owner = nullptr;
    if (!loader ||
        !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(loader), &owner))
        return nullptr;
    wchar_t path[32768];
    const DWORD length = GetModuleFileNameW(owner, path, 32768);
    if (!length || length >= 32768)
        return nullptr;
    const wchar_t* name = std::wcsrchr(path, L'\\');
    name = name ? name + 1 : path;
    const uintptr_t offset = reinterpret_cast<uintptr_t>(loader) - reinterpret_cast<uintptr_t>(owner);
    HANDLE raw = INVALID_HANDLE_VALUE;
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        raw = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (raw != INVALID_HANDLE_VALUE || GetLastError() != ERROR_BAD_LENGTH)
            break;
    }
    Handle snapshot(raw);
    if (raw == INVALID_HANDLE_VALUE)
        return nullptr;
    MODULEENTRY32W module = {};
    module.dwSize = sizeof(module);
    if (!Module32FirstW(raw, &module))
        return nullptr;
    do {
        if (!_wcsicmp(module.szModule, name) && offset < module.modBaseSize)
            return reinterpret_cast<LPTHREAD_START_ROUTINE>(module.modBaseAddr + offset);
    } while (Module32NextW(raw, &module));
    return nullptr;
}
DWORD Remaining(ULONGLONG start, DWORD timeout) {
    const ULONGLONG elapsed = GetTickCount64() - start;
    return elapsed >= timeout ? 0 : timeout - static_cast<DWORD>(elapsed);
}
bool InitializeLoader(HANDLE process, DWORD timeout) {
    // A new process has ntdll mapped, but may not have initialized its Win32 imports yet.
    // Thread startup performs that initialization before calling RtlExitUserThread.
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    const auto exitThread = GetProcAddress(ntdll, "RtlExitUserThread");
    MEMORY_BASIC_INFORMATION region = {};
    uint8_t observed[32];
    SIZE_T received = 0;
    if (!exitThread || !VirtualQueryEx(process, reinterpret_cast<const void*>(exitThread), &region, sizeof(region)) ||
        region.Type != MEM_IMAGE || region.State != MEM_COMMIT || region.AllocationBase != ntdll ||
        !ReadProcessMemory(process, reinterpret_cast<const void*>(exitThread), observed, sizeof(observed), &received) ||
        received != sizeof(observed) ||
        std::memcmp(observed, reinterpret_cast<const void*>(exitThread), sizeof(observed)))
        return false;
    Handle thread(CreateRemoteThread(process, nullptr, 0, reinterpret_cast<LPTHREAD_START_ROUTINE>(exitThread), nullptr,
                                     0, nullptr));
    if (!thread.Get())
        return false;
    HANDLE wait[] = {process, thread.Get()};
    return WaitForMultipleObjects(2, wait, FALSE, timeout) == WAIT_OBJECT_0 + 1;
}
} // namespace
std::wstring ClientReadyEvent(DWORD pid) {
    return L"Local\\GlideAholicReady-" + std::to_wstring(pid);
}
bool InjectAndWait(HANDLE process, const std::wstring& dllPath, DWORD timeoutMs, std::string& error) {
    error = "Cannot inject client DLL";
    const DWORD pid = GetProcessId(process);
    if (!pid || pid == GetCurrentProcessId())
        return false;
    const auto fail = [&](const char* reason) {
        error = reason;
        TerminateProcess(process, ERROR_ACCESS_DISABLED_BY_POLICY);
        WaitForSingleObject(process, 5000);
        return false;
    };
    if (!timeoutMs || timeoutMs == INFINITE || dllPath.size() < 3 || dllPath[1] != L':' ||
        (dllPath[2] != L'\\' && dllPath[2] != L'/') || dllPath.find(L'\0') != std::wstring::npos ||
        dllPath.size() >= 32767)
        return fail("Client DLL requires an absolute path and a finite preparation timeout");
    BOOL selfWow = FALSE, targetWow = FALSE;
    if (!IsWow64Process(GetCurrentProcess(), &selfWow) || !IsWow64Process(process, &targetWow) || selfWow != targetWow)
        return fail("Client injection requires a helper matching the target architecture");
    PeImage image;
    if (!PeImage::FromFile(dllPath.c_str(), image) ||
        image.Machine() != (sizeof(void*) == 8 ? Bitness::X64 : Bitness::X86))
        return fail("Client DLL is missing, invalid or has the wrong architecture");
    Handle ready(CreateEventW(nullptr, TRUE, FALSE, ClientReadyEvent(pid).c_str()));
    if (!ready.Get() || GetLastError() == ERROR_ALREADY_EXISTS)
        return fail("Client readiness event is already owned or unavailable");
    const ULONGLONG start = GetTickCount64();
    auto loader = RemoteLoader(pid);
    if (!loader && InitializeLoader(process, Remaining(start, timeoutMs)))
        loader = RemoteLoader(pid);
    if (!loader)
        return fail("Cannot locate the DLL loader in the target process");
    const SIZE_T size = (dllPath.size() + 1) * sizeof(wchar_t);
    void* remote = VirtualAllocEx(process, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote)
        return fail("Cannot allocate DLL path in target process");
    SIZE_T written = 0;
    if (!WriteProcessMemory(process, remote, dllPath.c_str(), size, &written) || written != size) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return fail("Cannot write DLL path in target process");
    }
    Handle thread(CreateRemoteThread(process, nullptr, 0, loader, remote, 0, nullptr));
    if (!thread.Get()) {
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        return fail("Cannot start DLL loader thread");
    }
    HANDLE loading[] = {process, thread.Get()};
    const DWORD loaded = WaitForMultipleObjects(2, loading, FALSE, Remaining(start, timeoutMs));
    if (loaded != WAIT_OBJECT_0 + 1)
        return fail("Client exited or DLL loading timed out");
    VirtualFreeEx(process, remote, 0, MEM_RELEASE);
    HANDLE preparation[] = {process, ready.Get()};
    if (WaitForMultipleObjects(2, preparation, FALSE, Remaining(start, timeoutMs)) != WAIT_OBJECT_0 + 1)
        return fail("Client exited or DLL did not signal readiness");
    error.clear();
    return true;
}
} // namespace glide
