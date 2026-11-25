// Shared June hotkey/thread handling, with retained own HoT offsets and signatures.
#include "exploration.h"
#include <Windows.h>

namespace glide {
namespace {
using ViewState = void(__fastcall*)(unsigned*, void*, unsigned);
using Dispatch = void(__fastcall*)(void*, void*, uintptr_t);
using Camera = void(__fastcall*)(void*, void*, unsigned);
using FogSubmit = void(__fastcall*)(const void*, const void*, const void*, const void*, const void*, uint32_t);
using FogDisable = void(__cdecl*)();
ViewState viewState = nullptr;
Dispatch dispatch = nullptr;
Camera camera = nullptr;
FogSubmit fogSubmit = nullptr;
FogDisable fogDisable = nullptr;
Log* clientLog = nullptr;
volatile LONG currentView = 0, pendingCamera = 0, requestedAt = 0, fogDisabled = 0;
bool cameraDown = false, fogDown = false;
bool Gameplay() {
    return InterlockedCompareExchange(&currentView, 0, 0) == 16;
}
bool Active() {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId() && Gameplay();
}
void __fastcall TrackView(unsigned* context, void* unused, unsigned state) {
    viewState(context, unused, state);
    const LONG actual = static_cast<LONG>(context[0x34 / 4]);
    const LONG previous = InterlockedExchange(&currentView, actual);
    if (previous != actual)
        clientLog->Write("hot-view", "Native view=" + std::to_string(actual));
    if (!Gameplay())
        InterlockedExchange(&pendingCamera, 0);
}
void ObserveKeys() {
    const bool active = Active();
    const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool cameraKey = control && (GetAsyncKeyState('F') & 0x8000);
    const bool fogKey = control && (GetAsyncKeyState('N') & 0x8000);
    if (active && cameraKey && !cameraDown)
        QueueHotCamera();
    if (active && fogKey && !fogDown)
        ToggleHotFog();
    cameraDown = cameraKey;
    fogDown = fogKey;
    if (!active)
        InterlockedExchange(&pendingCamera, 0);
}
void __fastcall GameplayDispatch(void* control, void* message, uintptr_t token) {
    ObserveKeys();
    if (InterlockedCompareExchange(&pendingCamera, 0, 0)) {
        const DWORD elapsed = GetTickCount() - static_cast<DWORD>(InterlockedCompareExchange(&requestedAt, 0, 0));
        // Consume only on the gameplay thread; never retain a scene or toggle
        // while its destruction event is being dispatched.
        if (elapsed > 1500 ||
            (static_cast<uint32_t*>(control)[1] == 0x0c || static_cast<uint32_t*>(control)[1] == 0x0d)) {
            InterlockedExchange(&pendingCamera, 0);
        } else {
            void** holder = *reinterpret_cast<void***>(static_cast<uint8_t*>(control) + 8);
            if (holder && *holder) {
                void* scene = *holder;
                const unsigned enabled = !(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(scene) + 0x1c8) & 2u);
                InterlockedExchange(&pendingCamera, 0);
                camera(scene, nullptr, enabled);
                clientLog->Write("camera", enabled ? "Free camera on (Ctrl+F)" : "Free camera off (Ctrl+F)");
            }
        }
    }
    dispatch(control, message, token);
}
void __fastcall SubmitFog(const void* a, const void* b, const void* c, const void* d, const void* e, uint32_t f) {
    // Use the native device command. Merely skipping submission retains old fog.
    if (Gameplay() && InterlockedCompareExchange(&fogDisabled, 0, 0))
        fogDisable();
    else
        fogSubmit(a, b, c, d, e, f);
}
} // namespace
void QueueHotCamera() {
    if (!Active())
        return;
    InterlockedExchange(&requestedAt, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&pendingCamera, 1);
}
void ToggleHotFog() {
    if (!Active())
        return;
    const LONG disabled = !InterlockedCompareExchange(&fogDisabled, 0, 0);
    InterlockedExchange(&fogDisabled, disabled);
    clientLog->Write("fog", disabled ? "Environment fog off (Ctrl+N)" : "Environment fog on (Ctrl+N)");
}
const std::vector<HotSymbol>& HotExplorationSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(hot_explore_viewState), SYMBOL(hot_explore_dispatch),
                                                   SYMBOL(hot_explore_cameraToggle), SYMBOL(hot_explore_fogSubmit),
                                                   SYMBOL(hot_explore_fogDisable)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotExplorationHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = HotExplorationSymbols();
        return std::vector<HookSpec>{{symbols[0].name, symbols[0].symbol, reinterpret_cast<void*>(TrackView),
                                      reinterpret_cast<void**>(&viewState), true},
                                     {symbols[1].name, symbols[1].symbol, reinterpret_cast<void*>(GameplayDispatch),
                                      reinterpret_cast<void**>(&dispatch), true},
                                     {symbols[3].name, symbols[3].symbol, reinterpret_cast<void*>(SubmitFog),
                                      reinterpret_cast<void**>(&fogSubmit), true}};
    }();
    return hooks;
}
bool PrepareHotExploration(const PeImage& image, void* module, Log& log, std::string& error) {
    error = "Hot exploration requires the verified x86 client";
    const BuildInfo* build = nullptr;
    if (!module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 ||
        DetectBuild(image, 49284, build) != BuildDetection::Found)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : HotExplorationSymbols()) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("Hot exploration RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(match.rva);
    }
    auto* base = static_cast<uint8_t*>(module);
    camera = reinterpret_cast<Camera>(base + offsets[2]);
    fogDisable = reinterpret_cast<FogDisable>(base + offsets[4]);
    clientLog = &log;
    error.clear();
    return true;
}
} // namespace glide
