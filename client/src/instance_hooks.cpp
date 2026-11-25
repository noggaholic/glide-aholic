#include "instance_hooks.h"
#include <Windows.h>
#include <cstdio>
#include <cwchar>
#include <cstring>

namespace glide {
namespace {
unsigned buildNumber = 0, slotNumber = 0;
decltype(&CreateMutexA) createAnsi = nullptr;
decltype(&CreateMutexW) createWide = nullptr;
decltype(&OpenMutexA) openAnsi = nullptr;
decltype(&OpenMutexW) openWide = nullptr;
thread_local unsigned depth = 0;
struct Call {
    Call() {
        ++depth;
    }
    ~Call() {
        --depth;
    }
};
const char* Scope(const char* name, char (&out)[512]) {
    if (depth || !name || _strnicmp(name, "AN-Mutex-", 9))
        return name;
    if (_snprintf_s(out, sizeof(out), _TRUNCATE, "%s-GlideAholic-%u-%u", name, buildNumber, slotNumber) < 0) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return nullptr;
    }
    return out;
}
const wchar_t* Scope(const wchar_t* name, wchar_t (&out)[512]) {
    if (depth || !name || _wcsnicmp(name, L"AN-Mutex-", 9))
        return name;
    if (_snwprintf_s(out, _countof(out), _TRUNCATE, L"%s-GlideAholic-%u-%u", name, buildNumber, slotNumber) < 0) {
        SetLastError(ERROR_FILENAME_EXCED_RANGE);
        return nullptr;
    }
    return out;
}
HANDLE WINAPI CreateAnsi(LPSECURITY_ATTRIBUTES security, BOOL owner, LPCSTR name) {
    char text[512];
    const auto scoped = Scope(name, text);
    if (name && !scoped)
        return nullptr;
    Call call;
    return createAnsi(security, owner, scoped);
}
HANDLE WINAPI CreateWide(LPSECURITY_ATTRIBUTES security, BOOL owner, LPCWSTR name) {
    wchar_t text[512];
    const auto scoped = Scope(name, text);
    if (name && !scoped)
        return nullptr;
    Call call;
    return createWide(security, owner, scoped);
}
HANDLE WINAPI OpenAnsi(DWORD access, BOOL inherit, LPCSTR name) {
    char text[512];
    const auto scoped = Scope(name, text);
    if (name && !scoped)
        return nullptr;
    Call call;
    return openAnsi(access, inherit, scoped);
}
HANDLE WINAPI OpenWide(DWORD access, BOOL inherit, LPCWSTR name) {
    wchar_t text[512];
    const auto scoped = Scope(name, text);
    if (name && !scoped)
        return nullptr;
    Call call;
    return openWide(access, inherit, scoped);
}
} // namespace
bool InstallInstanceHooks(Hooks& hooks, unsigned build, unsigned slot, std::string& error) {
    if (!build || !slot) {
        error = "Instance hooks require a build and reserved slot";
        return false;
    }
    buildNumber = build;
    slotNumber = slot;
#define ATTACH(api, detour, original)                                                                                  \
    if (!hooks.AttachExport(L"kernel32.dll", #api, reinterpret_cast<void*>(detour),                                    \
                            reinterpret_cast<void**>(&original), error))                                               \
    return false
    ATTACH(CreateMutexA, CreateAnsi, createAnsi);
    ATTACH(CreateMutexW, CreateWide, createWide);
    ATTACH(OpenMutexA, OpenAnsi, openAnsi);
    ATTACH(OpenMutexW, OpenWide, openWide);
#undef ATTACH
    error.clear();
    return true;
}
} // namespace glide
