#include "child_hooks.h"
#include "glide/injection.h"
#include "glide/log.h"
#include <winternl.h>
#include <cstring>
#include <cwchar>

namespace glide {
namespace {
ChildProtection configuration;
decltype(&CreateProcessW) originalWide = nullptr;
decltype(&CreateProcessA) originalAnsi = nullptr;
using NativeCreate = NTSTATUS(NTAPI*)(PHANDLE, PHANDLE, ACCESS_MASK, ACCESS_MASK, POBJECT_ATTRIBUTES,
                                      POBJECT_ATTRIBUTES, ULONG, ULONG, PVOID, PVOID, PVOID);
NativeCreate originalNative = nullptr;
thread_local unsigned authorizedSpawn = 0;
template <class Character> bool SnapshotEnvironment(const Character* source, Character* destination, size_t& length) {
    // CreateProcess accepts a pointer without a length. Bound reads and reject invalid pointers.
    __try {
        for (size_t i = 0; i < 32767; ++i) {
            destination[i] = source[i];
            if (i && !destination[i] && !destination[i - 1]) {
                length = i + 1;
                return true;
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
}
bool ChildEnvironment(void* supplied, DWORD flags, std::vector<wchar_t>& output) {
    Environment values;
    if (!supplied) {
        if (!ReadEnvironment(values))
            return false;
    } else {
        std::vector<wchar_t> text(32767);
        size_t length = 0;
        if (flags & CREATE_UNICODE_ENVIRONMENT) {
            if (!SnapshotEnvironment(static_cast<const wchar_t*>(supplied), text.data(), length))
                return false;
        } else {
            std::vector<char> ansi(32767);
            if (!SnapshotEnvironment(static_cast<const char*>(supplied), ansi.data(), length))
                return false;
            const int count = MultiByteToWideChar(CP_ACP, 0, ansi.data(), static_cast<int>(length), text.data(), 32767);
            if (count < 2)
                return false;
            length = static_cast<size_t>(count);
        }
        for (size_t at = 0; at + 1 < length && text[at];) {
            const std::wstring entry(text.data() + at);
            const size_t equals = entry.find(L'=', entry[0] == L'=' ? 1 : 0);
            if (equals == std::wstring::npos || !equals)
                return false;
            values[entry.substr(0, equals)] = entry.substr(equals + 1);
            at += entry.size() + 1;
        }
    }
    std::string error;
    return configuration.slot.MakeEnvironment(values, output, error);
}
bool Absolute(const std::wstring& value, std::wstring& out) {
    if (value.size() < 3 || value[1] != L':' || (value[2] != L'\\' && value[2] != L'/') ||
        value.find(L'\0') != std::wstring::npos)
        return false;
    wchar_t full[32768];
    const DWORD count = GetFullPathNameW(value.c_str(), 32768, full, nullptr);
    if (!count || count >= 32768)
        return false;
    out = full;
    return true;
}
void Diagnostic(const std::string& message) {
    const DWORD saved = GetLastError();
    if (configuration.log)
        configuration.log->Write("child", message);
    SetLastError(saved);
}
BOOL Denied(LPPROCESS_INFORMATION process) {
    if (process)
        std::memset(process, 0, sizeof(*process));
    SetLastError(ERROR_ACCESS_DISABLED_BY_POLICY);
    return FALSE;
}
bool Approved(HANDLE process) {
    wchar_t path[32768];
    DWORD size = 32768;
    if (!QueryFullProcessImageNameW(process, 0, path, &size))
        return false;
    std::wstring normalized;
    if (!Absolute(path, normalized))
        return false;
    for (const auto& allowed : configuration.executables)
        if (!_wcsicmp(normalized.c_str(), allowed.c_str()))
            return true;
    return false;
}
BOOL Finish(DWORD flags, LPPROCESS_INFORMATION process) {
    std::string error;
    if (!Approved(process->hProcess) ||
        !InjectAndWait(process->hProcess, configuration.dllPath, configuration.readyTimeoutMs, error) ||
        (!(flags & CREATE_SUSPENDED) && ResumeThread(process->hThread) == static_cast<DWORD>(-1))) {
        if (!error.empty())
            OutputDebugStringA(error.c_str());
        Diagnostic("Child approval/injection/resume failed: " + error);
        TerminateProcess(process->hProcess, ERROR_ACCESS_DISABLED_BY_POLICY);
        WaitForSingleObject(process->hProcess, 5000);
        CloseHandle(process->hThread);
        CloseHandle(process->hProcess);
        return Denied(process);
    }
    return TRUE;
}
BOOL WINAPI GuardWide(LPCWSTR app, LPWSTR command, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta, BOOL inherit,
                      DWORD flags, LPVOID environment, LPCWSTR directory, LPSTARTUPINFOW startup,
                      LPPROCESS_INFORMATION process) {
    if (authorizedSpawn)
        return originalWide(app, command, pa, ta, inherit, flags, environment, directory, startup, process);
    if (flags & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)) {
        Diagnostic("Rejected child creation flags=" + std::to_string(flags));
        return Denied(process);
    }
    // Keep legacy browser requests inside the parent's job. Actual image
    // approval and injection still happen before the child can execute.
    if (flags & CREATE_BREAKAWAY_FROM_JOB) {
        flags &= ~CREATE_BREAKAWAY_FROM_JOB;
        Diagnostic("Browser breakaway request removed; child remains in the instance job");
    }
    std::vector<wchar_t> childEnvironment;
    if (configuration.slot.build) {
        if (!ChildEnvironment(environment, flags, childEnvironment)) {
            Diagnostic("Invalid child environment");
            return Denied(process);
        }
        environment = childEnvironment.data();
        flags |= CREATE_UNICODE_ENVIRONMENT;
    }
    ++authorizedSpawn;
    const BOOL created =
        originalWide(app, command, pa, ta, inherit, flags | CREATE_SUSPENDED, environment, directory, startup, process);
    --authorizedSpawn;
    if (!created)
        Diagnostic("CreateProcess failed, error=" + std::to_string(GetLastError()));
    return created ? Finish(flags, process) : FALSE;
}
BOOL WINAPI GuardAnsi(LPCSTR app, LPSTR command, LPSECURITY_ATTRIBUTES pa, LPSECURITY_ATTRIBUTES ta, BOOL inherit,
                      DWORD flags, LPVOID environment, LPCSTR directory, LPSTARTUPINFOA startup,
                      LPPROCESS_INFORMATION process) {
    if (authorizedSpawn)
        return originalAnsi(app, command, pa, ta, inherit, flags, environment, directory, startup, process);
    if (flags & (DEBUG_PROCESS | DEBUG_ONLY_THIS_PROCESS)) {
        Diagnostic("Rejected child creation flags=" + std::to_string(flags));
        return Denied(process);
    }
    // Keep legacy browser requests inside the parent's job. Actual image
    // approval and injection still happen before the child can execute.
    if (flags & CREATE_BREAKAWAY_FROM_JOB) {
        flags &= ~CREATE_BREAKAWAY_FROM_JOB;
        Diagnostic("Browser breakaway request removed; child remains in the instance job");
    }
    std::vector<wchar_t> childEnvironment;
    if (configuration.slot.build) {
        if (!ChildEnvironment(environment, flags, childEnvironment)) {
            Diagnostic("Invalid child environment");
            return Denied(process);
        }
        environment = childEnvironment.data();
        flags |= CREATE_UNICODE_ENVIRONMENT;
    }
    ++authorizedSpawn;
    const BOOL created =
        originalAnsi(app, command, pa, ta, inherit, flags | CREATE_SUSPENDED, environment, directory, startup, process);
    --authorizedSpawn;
    if (!created)
        Diagnostic("CreateProcess failed, error=" + std::to_string(GetLastError()));
    return created ? Finish(flags, process) : FALSE;
}
NTSTATUS NTAPI GuardNative(PHANDLE process, PHANDLE thread, ACCESS_MASK pa, ACCESS_MASK ta, POBJECT_ATTRIBUTES po,
                           POBJECT_ATTRIBUTES to, ULONG pf, ULONG tf, PVOID parameters, PVOID info, PVOID attributes) {
    if (!authorizedSpawn) {
        Diagnostic("Rejected direct native process creation");
        return static_cast<NTSTATUS>(0xc0000022L);
    }
    return originalNative(process, thread, pa, ta, po, to, pf, tf, parameters, info, attributes);
}
} // namespace
bool InstallChildHooks(Hooks& hooks, const ChildProtection& protection, std::string& error) {
    error = "Cannot prepare browser child protection";
    if (originalWide || !protection.readyTimeoutMs || protection.readyTimeoutMs == INFINITE)
        return false;
    ChildProtection normalized;
    normalized.log = protection.log;
    normalized.readyTimeoutMs = protection.readyTimeoutMs;
    if (protection.slot.build) {
        std::vector<wchar_t> validated;
        if (!protection.slot.MakeEnvironment({}, validated, error))
            return false;
        normalized.slot = protection.slot;
    }
    if (!Absolute(protection.dllPath, normalized.dllPath))
        return false;
    for (const auto& executable : protection.executables) {
        std::wstring path;
        if (!Absolute(executable, path))
            return false;
        normalized.executables.push_back(std::move(path));
    }
    configuration = std::move(normalized);
    return hooks.AttachExport(L"kernel32.dll", "CreateProcessW", reinterpret_cast<void*>(GuardWide),
                              reinterpret_cast<void**>(&originalWide), error) &&
           hooks.AttachExport(L"kernel32.dll", "CreateProcessA", reinterpret_cast<void*>(GuardAnsi),
                              reinterpret_cast<void**>(&originalAnsi), error) &&
           hooks.AttachExport(L"ntdll.dll", "NtCreateUserProcess", reinterpret_cast<void*>(GuardNative),
                              reinterpret_cast<void**>(&originalNative), error);
}
} // namespace glide
