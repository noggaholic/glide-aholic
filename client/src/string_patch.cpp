#include "string_patch.h"
#include <Windows.h>
#include <cstring>
#include <vector>
#include <algorithm>

namespace glide {
namespace {
bool Read(const void* address, void* out, size_t size) {
    SIZE_T received = 0;
    return ReadProcessMemory(GetCurrentProcess(), address, out, size, &received) && received == size;
}
#ifdef _WIN64
void* AllocateNear(uintptr_t next, size_t size) {
    SYSTEM_INFO system;
    GetSystemInfo(&system);
    const uintptr_t granularity = system.dwAllocationGranularity;
    const uintptr_t minimum = (std::max)(reinterpret_cast<uintptr_t>(system.lpMinimumApplicationAddress),
                                         next > INT32_MAX ? next - INT32_MAX : uintptr_t(0));
    const uintptr_t maximum = (std::min)(reinterpret_cast<uintptr_t>(system.lpMaximumApplicationAddress),
                                         next > UINTPTR_MAX - INT32_MAX ? UINTPTR_MAX : next + INT32_MAX);
    uintptr_t at = minimum;
    while (at < maximum) {
        MEMORY_BASIC_INFORMATION memory = {};
        if (!VirtualQuery(reinterpret_cast<void*>(at), &memory, sizeof(memory)))
            break;
        const uintptr_t base = reinterpret_cast<uintptr_t>(memory.BaseAddress);
        if (memory.RegionSize > UINTPTR_MAX - base)
            break;
        const uintptr_t end = base + memory.RegionSize;
        const uintptr_t candidate = (at + granularity - 1) & ~(granularity - 1);
        if (memory.State == MEM_FREE && candidate >= at && candidate < maximum && candidate <= end &&
            size <= end - candidate && size <= maximum - candidate) {
            if (void* allocated =
                    VirtualAlloc(reinterpret_cast<void*>(candidate), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE))
                return allocated;
        }
        if (end <= at)
            break;
        at = end;
    }
    return nullptr;
}
#endif
bool WriteOperand(uint8_t* instruction, int32_t value, size_t offset = 3, size_t length = 7) {
    DWORD previous = 0, ignored = 0;
    if (!VirtualProtect(instruction, length, PAGE_EXECUTE_READWRITE, &previous))
        return false;
    SIZE_T written = 0;
    const bool wrote = WriteProcessMemory(GetCurrentProcess(), instruction + offset, &value, sizeof(value), &written) &&
                       written == sizeof(value);
    const bool flushed = FlushInstructionCache(GetCurrentProcess(), instruction, length) != FALSE;
    const bool restored = VirtualProtect(instruction, length, previous, &ignored) != FALSE;
    return wrote && flushed && restored;
}
} // namespace
RelativeStringPatch::~RelativeStringPatch() {
    std::string error;
    // Keep backing memory if restoration fails; a remaining reference must stay valid.
    Reset(error);
}
bool RelativeStringPatch::Apply(void* instruction, const std::wstring& expected, const std::wstring& replacement,
                                std::string& error) {
    error = "Invalid x64 launcher string patch";
#ifndef _WIN64
    (void)instruction;
    (void)expected;
    (void)replacement;
    return false;
#else
    if (instruction_ || !instruction || expected.empty() || replacement.empty() || expected.size() > 32767 ||
        replacement.size() > 32767 || expected.find(L'\0') != std::wstring::npos ||
        replacement.find(L'\0') != std::wstring::npos)
        return false;
    uint8_t code[7];
    if (!Read(instruction, code, sizeof(code)) || (code[0] != 0x48 && code[0] != 0x4c) || code[1] != 0x8d ||
        (code[2] & 0xc7) != 5)
        return false;
    int32_t displacement;
    std::memcpy(&displacement, code + 3, sizeof(displacement));
    const uintptr_t next = reinterpret_cast<uintptr_t>(instruction) + 7;
    const int64_t target = static_cast<int64_t>(next) + displacement;
    if (target <= 0)
        return false;
    std::vector<wchar_t> original(expected.size() + 1);
    if (!Read(reinterpret_cast<void*>(static_cast<uintptr_t>(target)), original.data(),
              original.size() * sizeof(wchar_t)) ||
        std::memcmp(original.data(), expected.c_str(), original.size() * sizeof(wchar_t))) {
        error = "Launcher instruction does not reference the expected URL";
        return false;
    }
    const size_t size = (replacement.size() + 1) * sizeof(wchar_t);
    void* allocation = AllocateNear(next, size);
    if (!allocation) {
        error = "Cannot allocate launcher URL within instruction range";
        return false;
    }
    std::memcpy(allocation, replacement.c_str(), size);
    DWORD ignored = 0;
    if (!VirtualProtect(allocation, size, PAGE_READONLY, &ignored)) {
        VirtualFree(allocation, 0, MEM_RELEASE);
        error = "Cannot protect launcher URL storage";
        return false;
    }
    instruction_ = static_cast<uint8_t*>(instruction);
    allocation_ = allocation;
    original_ = displacement;
    operandOffset_ = 3;
    instructionSize_ = 7;
    replacement_ = static_cast<int32_t>(static_cast<int64_t>(reinterpret_cast<uintptr_t>(allocation)) -
                                        static_cast<int64_t>(next));
    if (!WriteOperand(instruction_, replacement_)) {
        std::string restoreError;
        Reset(restoreError);
        error = "Cannot patch launcher URL instruction";
        return false;
    }
    error.clear();
    return true;
#endif
}
bool RelativeStringPatch::ApplyPush32(void* instruction, const std::wstring& expected, const std::wstring& replacement,
                                      std::string& error) {
    error = "Invalid x86 launcher string patch";
#ifdef _WIN64
    (void)instruction;
    (void)expected;
    (void)replacement;
    return false;
#else
    if (instruction_ || !instruction || expected.empty() || replacement.empty() || expected.size() > 32767 ||
        replacement.size() > 32767 || expected.find(L'\0') != std::wstring::npos ||
        replacement.find(L'\0') != std::wstring::npos)
        return false;
    uint8_t code[5];
    if (!Read(instruction, code, sizeof(code)) || code[0] != 0x68)
        return false;
    uint32_t target = 0;
    std::memcpy(&target, code + 1, sizeof(target));
    std::vector<wchar_t> original(expected.size() + 1);
    if (!target || !Read(reinterpret_cast<void*>(target), original.data(), original.size() * sizeof(wchar_t)) ||
        std::memcmp(original.data(), expected.c_str(), original.size() * sizeof(wchar_t))) {
        error = "Launcher PUSH does not reference the expected URL";
        return false;
    }
    const size_t size = (replacement.size() + 1) * sizeof(wchar_t);
    void* allocation = VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!allocation) {
        error = "Cannot allocate x86 launcher URL";
        return false;
    }
    std::memcpy(allocation, replacement.c_str(), size);
    DWORD ignored = 0;
    if (!VirtualProtect(allocation, size, PAGE_READONLY, &ignored)) {
        VirtualFree(allocation, 0, MEM_RELEASE);
        error = "Cannot protect launcher URL storage";
        return false;
    }
    instruction_ = static_cast<uint8_t*>(instruction);
    allocation_ = allocation;
    std::memcpy(&original_, &target, sizeof(target));
    target = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(allocation));
    std::memcpy(&replacement_, &target, sizeof(target));
    operandOffset_ = 1;
    instructionSize_ = 5;
    if (!WriteOperand(instruction_, replacement_, operandOffset_, instructionSize_)) {
        std::string restoreError;
        Reset(restoreError);
        error = "Cannot patch launcher PUSH instruction";
        return false;
    }
    error.clear();
    return true;
#endif
}
bool RelativeStringPatch::Reset(std::string& error) {
    if (!instruction_) {
        error.clear();
        return true;
    }
    int32_t current = 0;
    error = "Cannot restore launcher URL instruction";
    if (!Read(instruction_ + operandOffset_, &current, sizeof(current)) ||
        (current != original_ && current != replacement_))
        return false;
    if (current != original_ && !WriteOperand(instruction_, original_, operandOffset_, instructionSize_))
        return false;
    if (!VirtualFree(allocation_, 0, MEM_RELEASE))
        return false;
    allocation_ = nullptr;
    instruction_ = nullptr;
    error.clear();
    return true;
}
} // namespace glide
