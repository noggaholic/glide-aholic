#pragma once
#include <string>
#include <cstdint>

namespace glide {
// Startup-only x64 LEA or x86 PUSH string patch; owns the replacement storage.
class RelativeStringPatch {
  public:
    ~RelativeStringPatch();
    RelativeStringPatch() = default;
    RelativeStringPatch(const RelativeStringPatch&) = delete;
    RelativeStringPatch& operator=(const RelativeStringPatch&) = delete;
    bool Apply(void* instruction, const std::wstring& expected, const std::wstring& replacement, std::string& error);
    bool ApplyPush32(void* instruction, const std::wstring& expected, const std::wstring& replacement,
                     std::string& error);
    bool Reset(std::string& error);

  private:
    uint8_t* instruction_ = nullptr;
    void* allocation_ = nullptr;
    size_t operandOffset_ = 3, instructionSize_ = 7;
    int32_t original_ = 0, replacement_ = 0;
};
} // namespace glide
