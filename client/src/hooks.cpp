#include "hooks.h"
#include "../../vendor/minhook/include/MinHook.h"
#include <Windows.h>

namespace glide {
Hooks::~Hooks() {
    if (initialized_) {
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();
        for (void** original : originals_)
            *original = nullptr;
    }
}
bool Hooks::Initialize(std::string& error) {
    error = "Cannot initialize function hooks";
    if (initialized_ || MH_Initialize() != MH_OK)
        return false;
    initialized_ = true;
    error.clear();
    return true;
}
bool Hooks::Create(void* target, const char* name, void* detour, void** original, std::string& error) {
    error = std::string("Cannot prepare hook: ") + (name ? name : "unnamed");
    if (!initialized_ || enabled_ || failed_ || !target || !detour || !original || *original) {
        failed_ = true;
        return false;
    }
    void* trampoline = nullptr;
    const MH_STATUS status = MH_CreateHook(target, detour, &trampoline);
    if (status != MH_OK) {
        error += " (" + std::string(MH_StatusToString(status)) + ")";
        failed_ = true;
        return false;
    }
    *original = trampoline;
    originals_.push_back(original);
    error.clear();
    return true;
}
bool Hooks::Attach(const PeImage& image, void* module, const HookSpec& spec, std::string& error) {
    if (!initialized_ || enabled_ || failed_) {
        error = "Hook installation is not available";
        return false;
    }
    const auto match = ResolveSymbol(image, spec.symbol);
    if (!match.count && !spec.required) {
        error.clear();
        return true;
    }
    if (match.count != 1 || !module) {
        error = std::string("Hook RVA must belong to the verified executable: ") + (spec.name ? spec.name : "unnamed");
        failed_ = true;
        return false;
    }
    return Create(static_cast<uint8_t*>(module) + match.rva, spec.name, spec.detour, spec.original, error);
}
bool Hooks::AttachExport(const wchar_t* library, const char* name, void* detour, void** original, std::string& error) {
    HMODULE module = library ? GetModuleHandleW(library) : nullptr;
    void* target = module && name ? reinterpret_cast<void*>(GetProcAddress(module, name)) : nullptr;
    return Create(target, name, detour, original, error);
}
bool Hooks::Enable(std::string& error) {
    error = "Cannot enable function hooks";
    if (!initialized_ || enabled_ || failed_ || originals_.empty())
        return false;
    const MH_STATUS status = MH_EnableHook(MH_ALL_HOOKS);
    if (status != MH_OK) {
        MH_DisableHook(MH_ALL_HOOKS);
        failed_ = true;
        error += " (" + std::string(MH_StatusToString(status)) + ")";
        return false;
    }
    enabled_ = true;
    error.clear();
    return true;
}
} // namespace glide
