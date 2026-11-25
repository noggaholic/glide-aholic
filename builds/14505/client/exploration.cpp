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
uint32_t cameraFlagsOffset = 0x13c;
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
    InterlockedExchange(&currentView, static_cast<LONG>(context[0x45]));
    if (!Gameplay())
        InterlockedExchange(&pendingCamera, 0);
}
void ObserveKeys() {
    const bool active = Active();
    const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool cameraKey = control && (GetAsyncKeyState('F') & 0x8000);
    const bool fogKey = control && (GetAsyncKeyState('N') & 0x8000);
    if (active && cameraKey && !cameraDown)
        QueueJuneCamera();
    if (active && fogKey && !fogDown)
        ToggleJuneFog();
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
        if (elapsed > 1500 || static_cast<uint32_t*>(control)[1] == 0x0c) {
            InterlockedExchange(&pendingCamera, 0);
        } else {
            void** holder = *reinterpret_cast<void***>(static_cast<uint8_t*>(control) + 8);
            if (holder && *holder) {
                void* scene = *holder;
                const unsigned enabled =
                    !(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(scene) + cameraFlagsOffset) & 1u);
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
void QueueJuneCamera() {
    if (!Active())
        return;
    InterlockedExchange(&requestedAt, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&pendingCamera, 1);
}
void ToggleJuneFog() {
    if (!Active())
        return;
    const LONG disabled = !InterlockedCompareExchange(&fogDisabled, 0, 0);
    InterlockedExchange(&fogDisabled, disabled);
    clientLog->Write("fog", disabled ? "Environment fog off (Ctrl+N)" : "Environment fog on (Ctrl+N)");
}
const std::vector<JuneSymbol>& JuneExplorationSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<JuneSymbol> symbols = {
        SYMBOL(client_2012_view_state), SYMBOL(client_2012_gameplay_dispatch), SYMBOL(client_2012_debug_camera_toggle),
        SYMBOL(client_2012_environment_fog_submit), SYMBOL(client_2012_environment_fog_disable)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& JuneExplorationHooks() {
    static const std::vector<HookSpec> hooks = MakeJuneExplorationHooks(JuneExplorationSymbols());
    return hooks;
}
std::vector<HookSpec> MakeJuneExplorationHooks(const std::vector<JuneSymbol>& symbols) {
    if (symbols.size() != 5)
        return {};
    return std::vector<HookSpec>{{symbols[0].name, symbols[0].symbol, reinterpret_cast<void*>(TrackView),
                                  reinterpret_cast<void**>(&viewState), true},
                                 {symbols[1].name, symbols[1].symbol, reinterpret_cast<void*>(GameplayDispatch),
                                  reinterpret_cast<void**>(&dispatch), true},
                                 {symbols[3].name, symbols[3].symbol, reinterpret_cast<void*>(SubmitFog),
                                  reinterpret_cast<void**>(&fogSubmit), true}};
}
bool PrepareJuneExploration(const PeImage& image, void* module, Log& log, std::string& error) {
    return PrepareJuneExplorationFromSymbols(image, module, log, error, 14505, JuneExplorationSymbols());
}
bool PrepareJuneExplorationFromSymbols(const PeImage& image, void* module, Log& log, std::string& error,
                                       uint32_t buildNumber, const std::vector<JuneSymbol>& symbols) {
    error = "June exploration requires the matching 14422 or 14505 x86 client";
    if (!module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 ||
        (buildNumber != 14422 && buildNumber != 14505) || symbols.size() != 5)
        return false;
    const auto* build = BuildTable::Find(buildNumber);
    if (!build || ResolveSymbol(image, build->buildGetter).count != 1)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("June exploration RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(match.rva);
    }
    auto* base = static_cast<uint8_t*>(module);
    camera = reinterpret_cast<Camera>(base + offsets[2]);
    fogDisable = reinterpret_cast<FogDisable>(base + offsets[4]);
    cameraFlagsOffset = buildNumber == 14422 ? 0x134u : 0x13cu;
    clientLog = &log;
    error.clear();
    return true;
}
} // namespace glide
