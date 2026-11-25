#pragma once
#include "glide/symbols.h"
#include <string>
#include <vector>

namespace glide {
struct HookSpec {
    const char* name;
    SymbolReference symbol;
    void* detour;
    void** original;
    bool required;
};
// One owner per process. Install while startup is suspended; destroy only when detours are quiescent.
class Hooks {
  public:
    Hooks() = default;
    ~Hooks();
    Hooks(const Hooks&) = delete;
    Hooks& operator=(const Hooks&) = delete;
    bool Initialize(std::string& error);
    bool Attach(const PeImage& image, void* module, const HookSpec& spec, std::string& error);
    bool AttachExport(const wchar_t* library, const char* name, void* detour, void** original, std::string& error);
    bool Enable(std::string& error);

  private:
    bool Create(void* target, const char* name, void* detour, void** original, std::string& error);
    bool initialized_ = false, enabled_ = false, failed_ = false;
    std::vector<void**> originals_;
};
} // namespace glide
