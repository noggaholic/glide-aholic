#include "glide/runtime_field.h"
#include "controls.h"
#include "../../../client/src/runtime.h"
#include <Windows.h>
#include <array>

namespace glide {
namespace {
HotProfessionAvailable originalAvailable = nullptr;
HotProfessionContext profession = nullptr, originalValidation = nullptr;
HotMovementInput originalInput = nullptr;
HotMovementEligible movementEligible = nullptr;
HotGlideApply originalApply = nullptr;
HotGlidePhysics originalPhysics = nullptr;
Log* controlLog = nullptr;
bool prepared = false;
int __fastcall Available(unsigned value, void* unused) {
    return HotLocalProfession(value, unused, originalAvailable);
}
unsigned __fastcall Validate(void* context, void* unused) {
    return HotLocalProfessionValidation(context, unused, profession, originalValidation);
}
void __fastcall Input(void* self, void* unused, int value) {
    if ((value == 9 || value == 13) && self && controlLog) {
        void* character = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0xe0);
        const DWORD error = GetLastError();
        controlLog->Write("hot-glider-input", "character=" + std::to_string(reinterpret_cast<uintptr_t>(character)) +
                                                  " input=" + std::to_string(value));
        SetLastError(error);
    }
    DispatchHotGliderInput(self, unused, value, movementEligible, originalInput);
}
void __fastcall Apply(void* self, void* unused, const uint8_t* state) {
    ObserveHotGlideApply(self, unused, state, originalApply, *controlLog);
}
void __fastcall Physics(void* self, void* unused, float value) {
    ObserveHotGlidePhysics(self, unused, value, originalPhysics, *controlLog);
}
} // namespace
void ObserveHotGlideApply(void* self, void* unused, const uint8_t* state, HotGlideApply original, Log& log) {
    auto* agent = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(self) + 0x40);
    original(self, unused, state);
    const DWORD error = GetLastError();
    log.Write(
        "hot-glider-apply",
        "character=" + std::to_string(reinterpret_cast<uintptr_t>(self)) +
            " agent=" + std::to_string(reinterpret_cast<uintptr_t>(agent)) + " active=" + std::to_string(state[1]) +
            " characterFlags=" + std::to_string(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(self) + 0x88)) +
            " agentFlags=" + std::to_string(agent ? *reinterpret_cast<uint16_t*>(agent + 0x5c) : 0) +
            " physicsActive=" + std::to_string(agent ? *reinterpret_cast<uint32_t*>(agent + 0x80) : 0));
    SetLastError(error);
}
void ObserveHotGlidePhysics(void* self, void* unused, float value, HotGlidePhysics original, Log& log) {
    original(self, unused, value);
    const DWORD error = GetLastError();
    log.Write("hot-glider-physics",
              "agent=" + std::to_string(reinterpret_cast<uintptr_t>(self)) + " value=" + std::to_string(value) +
                  " active=" + std::to_string(*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(self) + 0x80)) +
                  " delta=" + std::to_string(glide::ReadRuntimeField<float>(self, 0xb4)));
    SetLastError(error);
}
int HotLocalProfession(unsigned value, void* unused, HotProfessionAvailable original) {
    return value == 9 ? 1 : original(value, unused);
}
unsigned HotLocalProfessionValidation(void* context, void* unused, HotProfessionContext selected,
                                      HotProfessionContext original) {
    return selected(context, unused) == 9 ? 0 : original(context, unused);
}
void DispatchHotGliderInput(void* self, void* unused, int input, HotMovementEligible eligible,
                            HotMovementInput original) {
    using Object = void*(__fastcall*)(void*, void*);
    using Check = int(__fastcall*)(void*, void*);
    using Action = void(__fastcall*)(void*, void*);
    if (input == 9 || input == 13) {
        void* character = *reinterpret_cast<void**>(static_cast<uint8_t*>(self) + 0xe0);
        if (character && eligible()) {
            void** table = *static_cast<void***>(character);
            const int active = reinterpret_cast<Check>(table[0x14c / 4])(character, nullptr);
            void* agent = reinterpret_cast<Object>(table[0xa8 / 4])(character, nullptr);
            if ((active && input == 13) ||
                (!active && agent &&
                 reinterpret_cast<Check>((*static_cast<void***>(agent))[0x1b8 / 4])(agent, nullptr))) {
                void* actions = reinterpret_cast<Object>(table[0x44 / 4])(character, nullptr);
                reinterpret_cast<Action>((*static_cast<void***>(actions))[0x74 / 4])(actions, nullptr);
                return;
            }
        }
    }
    original(self, unused, input);
}
const std::vector<HotSymbol>& HotControlSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(hot_creation_profession), SYMBOL(hot_creation_available),
                                                   SYMBOL(hot_creation_validate),   SYMBOL(hot_glide_eligible),
                                                   SYMBOL(hot_glide_dispatch),      SYMBOL(hot_glide_apply),
                                                   SYMBOL(hot_glide_physics)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotControlHooks() {
    static const std::vector<HookSpec> hooks = {
        {HotControlSymbols()[1].name, HotControlSymbols()[1].symbol, reinterpret_cast<void*>(Available),
         reinterpret_cast<void**>(&originalAvailable), true},
        {HotControlSymbols()[2].name, HotControlSymbols()[2].symbol, reinterpret_cast<void*>(Validate),
         reinterpret_cast<void**>(&originalValidation), true},
        {HotControlSymbols()[4].name, HotControlSymbols()[4].symbol, reinterpret_cast<void*>(Input),
         reinterpret_cast<void**>(&originalInput), true},
        {HotControlSymbols()[5].name, HotControlSymbols()[5].symbol, reinterpret_cast<void*>(Apply),
         reinterpret_cast<void**>(&originalApply), true},
        {HotControlSymbols()[6].name, HotControlSymbols()[6].symbol, reinterpret_cast<void*>(Physics),
         reinterpret_cast<void**>(&originalPhysics), true}};
    return hooks;
}
bool PrepareHotControls(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT controls require an unprepared x86 build 49284 client";
    if (prepared || !module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 || runtime.config.build != 49284)
        return false;
    std::array<uint32_t, 7> offsets;
    for (size_t i = 0; i < offsets.size(); ++i) {
        const auto& symbol = HotControlSymbols()[i];
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("HoT control RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets[i] = match.rva;
    }
    auto* base = static_cast<uint8_t*>(module);
    profession = reinterpret_cast<HotProfessionContext>(base + offsets[0]);
    movementEligible = reinterpret_cast<HotMovementEligible>(base + offsets[3]);
    controlLog = &runtime.log;
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
