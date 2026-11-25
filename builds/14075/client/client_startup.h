#pragma once
#include "native_resources.h"
#include "glide/log.h"
#include <array>

namespace glide {
namespace april14075 {
using AssignArrayFn = void(__fastcall*)(void*, void*, const void*, unsigned);
using CameraFn = void(__fastcall*)(void*, void*, unsigned);
using FogDisableFn = void(__cdecl*)();
struct NativeClientInputs {
    uint32_t build = 0;
    AssignArrayFn assign = nullptr;
    CameraFn camera = nullptr;
    FogDisableFn fog = nullptr;
    void** wrapper = nullptr;
    void** launcher = nullptr;
    Log* log = nullptr;
    void* platform = nullptr;
    bool (*foreground)(void*) = nullptr;
    bool (*key)(void*, unsigned) = nullptr;
    uint32_t (*tick)(void*) = nullptr;
    void (*pause)(void*, unsigned) = nullptr;
    bool (*ready)(void*) = nullptr;
};
struct StartupPatch {
    uint32_t rva = 0;
    std::array<uint8_t, 5> expected{}, replacement{};
    size_t size = 0;
};
bool ConfigureNativeClient(const NativeClientInputs&, std::string& error);
const std::vector<StartupSymbol>& ClientStartupSymbols();
const std::vector<HookSpec>& ClientStartupHooks();
bool SplashPatch(ByteView bytes, StartupPatch& out);
bool CoordinatorPatch(ByteView bytes, StartupPatch& out);
bool PlanClientPatches(const PeImage&, std::vector<StartupPatch>& out, std::string& error);
bool PrepareClientStartup(ClientRuntime&, const PeImage&, void* module, std::string& error);
bool FinishClientStartup(std::string& error);
unsigned RunAutoPlay();
void QueueCamera();
void ToggleFog();
const ClientProfile& ClientProfile14075();
} // namespace april14075
} // namespace glide
