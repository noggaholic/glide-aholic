#include "client_startup.h"
#include "../../../client/src/runtime.h"
#include "../../../client/src/instance_hooks.h"
#include <Windows.h>
#include <cstring>

namespace glide {
namespace april14075 {
namespace {
void* preparedModule = nullptr;
std::vector<StartupPatch> patches;
bool ImageSlot(void* module, uintptr_t slot) {
    MEMORY_BASIC_INFORMATION region = {};
    return VirtualQuery(reinterpret_cast<void*>(slot), &region, sizeof(region)) && region.AllocationBase == module &&
           region.State == MEM_COMMIT && !(region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) &&
           (region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) &&
           slot + sizeof(void*) <= reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
}
bool WritePatch(void* destination, const uint8_t* bytes, size_t size) {
    DWORD protection = 0, discard = 0;
    if (!VirtualProtect(destination, size, PAGE_EXECUTE_READWRITE, &protection))
        return false;
    std::memcpy(destination, bytes, size);
    const BOOL flushed = FlushInstructionCache(GetCurrentProcess(), destination, size);
    const BOOL restored = VirtualProtect(destination, size, protection, &discard);
    return flushed && restored;
}
DWORD WINAPI AutoPlay(void*) {
    return RunAutoPlay();
}
} // namespace
bool PrepareClientStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "April client startup requires matching x86 configuration";
    if (!module || sizeof(void*) != 4 || runtime.config.build != 14075 || image.Machine() != Bitness::X86)
        return false;
    const auto& symbols = ClientStartupSymbols();
    if (symbols.size() != 13)
        return false;
    std::array<uint32_t, 13> offsets{};
    for (size_t i = 0; i < symbols.size(); ++i) {
        const auto match = ResolveSymbol(image, symbols[i].symbol);
        if (match.count != 1) {
            error = std::string("April client startup RVA is not verified: ") + symbols[i].name;
            return false;
        }
        offsets[i] = match.rva;
    }
    std::vector<StartupPatch> candidate;
    if (!PlanClientPatches(image, candidate, error))
        return false;
    auto* base = static_cast<uint8_t*>(module);
    uint32_t wrapper = 0, launcher = 0;
    std::memcpy(&wrapper, base + offsets[5] + 2, 4);
    std::memcpy(&launcher, base + offsets[6] + 30, 4);
    error = "April launcher slots are outside writable image data";
    if (!ImageSlot(module, wrapper) || !ImageSlot(module, launcher))
        return false;
    NativeClientInputs selected;
    selected.build = 14075;
    selected.assign = reinterpret_cast<AssignArrayFn>(base + offsets[1]);
    selected.camera = reinterpret_cast<CameraFn>(base + offsets[9]);
    selected.fog = reinterpret_cast<FogDisableFn>(base + offsets[11]);
    selected.wrapper = reinterpret_cast<void**>(uintptr_t(wrapper));
    selected.launcher = reinterpret_cast<void**>(uintptr_t(launcher));
    selected.log = &runtime.log;
    if (!PrepareNativeResources(runtime, image, module, error) || !ConfigureNativeClient(selected, error) ||
        !InstallInstanceHooks(runtime.hooks, 14075, runtime.config.slot, error))
        return false;
    patches = std::move(candidate);
    preparedModule = module;
    error.clear();
    return true;
}
bool FinishClientStartup(std::string& error) {
    error = "April startup patches are not prepared";
    if (!preparedModule || patches.size() != 2)
        return false;
    auto* base = static_cast<uint8_t*>(preparedModule);
    for (const auto& patch : patches)
        if (std::memcmp(base + patch.rva, patch.expected.data(), patch.size))
            return false;
    size_t applied = 0;
    for (; applied < patches.size(); ++applied)
        if (!WritePatch(base + patches[applied].rva, patches[applied].replacement.data(), patches[applied].size))
            break;
    if (applied != patches.size()) {
        for (size_t i = 0; i <= applied && i < patches.size(); ++i)
            WritePatch(base + patches[i].rva, patches[i].expected.data(), patches[i].size);
        error = "Cannot apply April local startup instruction changes";
        return false;
    }
    HANDLE worker = CreateThread(nullptr, 0, AutoPlay, nullptr, 0, nullptr);
    if (!worker) {
        for (const auto& patch : patches)
            WritePatch(base + patch.rva, patch.expected.data(), patch.size);
        error = "Cannot start April automatic Play worker";
        return false;
    }
    CloseHandle(worker);
    error.clear();
    return true;
}
} // namespace april14075
} // namespace glide
