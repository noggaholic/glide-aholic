#include "exploration.h"
#include <Windows.h>
#include <cstdio>

namespace glide {
namespace {
using ViewState = void(__fastcall*)(unsigned*, void*, unsigned);
using Dispatch = void(__fastcall*)(void*, void*, uintptr_t);
using Camera = void(__fastcall*)(void*, void*, unsigned);
using FogSubmit = void(__fastcall*)(const void*, const void*, const void*, const void*, const void*, uint32_t);
using FogDisable = void(__cdecl*)();
using SkillGetter = uint32_t(__fastcall*)(void*, void*, unsigned);
ViewState viewState = nullptr;
Dispatch dispatch = nullptr;
Camera camera = nullptr;
FogSubmit fogSubmit = nullptr;
FogDisable fogDisable = nullptr;
SkillGetter skillGetter = nullptr;
Log* clientLog = nullptr;
volatile LONG currentView = 0, pendingCamera = 0, requestedAt = 0, fogDisabled = 0;
bool cameraDown = false, fogDown = false;
volatile LONG skillObservationEpoch = 0, skillObservationCount = 0;
bool SkillDiagnostics() {
    char value[2]{};
    return GetEnvironmentVariableA("GW2_JULY_SKILLBAR_DIAG", value, sizeof(value)) == 1 && value[0] == '1';
}
const JulySymbol& SkillDiagnosticSymbol() {
    static const JulySymbol symbol = {"july_skillbarGetter", {"july_skillbarGetter"}};
    return symbol;
}
uint32_t __fastcall ObserveSkillGetter(void* bar, void* unused, unsigned slot) {
    const uint32_t value = skillGetter(bar, unused, slot);
    if (slot >= 5 && slot <= 7) {
        struct Observation {
            void* bar = nullptr;
            uint32_t value = 0;
            LONG epoch = -1;
        };
        static Observation observed[3];
        auto& previous = observed[slot - 5];
        const LONG epoch = InterlockedCompareExchange(&skillObservationEpoch, 0, 0);
        if (previous.bar != bar || previous.value != value || previous.epoch != epoch) {
            previous = {bar, value, epoch};
            if (InterlockedIncrement(&skillObservationCount) <= 48) {
                char text[128]{};
                std::snprintf(text, sizeof(text), "epoch=%ld bar=%p slot=%u value=%u", epoch, bar, slot, value);
                clientLog->Write("skillbar", text);
            }
        }
    }
    return value;
}
bool Gameplay() {
    return InterlockedCompareExchange(&currentView, 0, 0) == 15;
}
bool Active() {
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId() && Gameplay();
}
void __fastcall TrackView(unsigned* context, void* unused, unsigned state) {
    viewState(context, unused, state);
    InterlockedExchange(&currentView, static_cast<LONG>(context[0x30 / 4]));
    if (!Gameplay())
        InterlockedExchange(&pendingCamera, 0);
}
void ObserveKeys() {
    const bool active = Active();
    const bool control = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    const bool cameraKey = control && (GetAsyncKeyState('F') & 0x8000);
    const bool fogKey = control && (GetAsyncKeyState('N') & 0x8000);
    if (active && cameraKey && !cameraDown)
        QueueJulyCamera();
    if (active && fogKey && !fogDown)
        ToggleJulyFog();
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
                const unsigned enabled = !(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(scene) + 0x140) & 1u);
                InterlockedExchange(&pendingCamera, 0);
                InterlockedIncrement(&skillObservationEpoch);
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
void QueueJulyCamera() {
    if (!Active())
        return;
    InterlockedExchange(&requestedAt, static_cast<LONG>(GetTickCount()));
    InterlockedExchange(&pendingCamera, 1);
}
void ToggleJulyFog() {
    if (!Active())
        return;
    const LONG disabled = !InterlockedCompareExchange(&fogDisabled, 0, 0);
    InterlockedExchange(&fogDisabled, disabled);
    clientLog->Write("fog", disabled ? "Environment fog off (Ctrl+N)" : "Environment fog on (Ctrl+N)");
}
const std::vector<JulySymbol>& JulyExplorationSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<JulySymbol> symbols = {SYMBOL(july_viewState), SYMBOL(july_explorationDispatch),
                                                    SYMBOL(july_cameraToggle), SYMBOL(july_fogSubmit),
                                                    SYMBOL(july_fogDisable)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& JulyExplorationHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = JulyExplorationSymbols();
        auto result =
            std::vector<HookSpec>{{symbols[0].name, symbols[0].symbol, reinterpret_cast<void*>(TrackView),
                                   reinterpret_cast<void**>(&viewState), true},
                                  {symbols[1].name, symbols[1].symbol, reinterpret_cast<void*>(GameplayDispatch),
                                   reinterpret_cast<void**>(&dispatch), true},
                                  {symbols[3].name, symbols[3].symbol, reinterpret_cast<void*>(SubmitFog),
                                   reinterpret_cast<void**>(&fogSubmit), true}};
        if (SkillDiagnostics()) {
            const auto& symbol = SkillDiagnosticSymbol();
            result.push_back({symbol.name, symbol.symbol, reinterpret_cast<void*>(ObserveSkillGetter),
                              reinterpret_cast<void**>(&skillGetter), true});
        }
        return result;
    }();
    return hooks;
}
bool PrepareJulyExploration(const PeImage& image, void* module, Log& log, std::string& error) {
    error = "July exploration requires the verified x86 client";
    if (!module || sizeof(void*) != 4 || image.Machine() != Bitness::X86)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : JulyExplorationSymbols()) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("July exploration RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(match.rva);
    }
    auto* base = static_cast<uint8_t*>(module);
    if (SkillDiagnostics()) {
        const auto match = ResolveSymbol(image, SkillDiagnosticSymbol().symbol);
        if (match.count != 1) {
            error = "July skillbar diagnostic RVA is not verified";
            return false;
        }
        log.Write("skillbar",
                  "Read-only skillbar diagnostic enabled; original getter calls and return values preserved");
    }
    camera = reinterpret_cast<Camera>(base + offsets[2]);
    fogDisable = reinterpret_cast<FogDisable>(base + offsets[4]);
    clientLog = &log;
    error.clear();
    return true;
}
} // namespace glide
