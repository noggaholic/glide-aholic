#include "glide/runtime_field.h"
#include "cartography.h"
#include "cartography_symbols.h"
#include <Windows.h>
namespace glide {
namespace china167409 {
namespace {
using Transform = float*(__fastcall*)(void*, float*, const float*, const float*);
using Rectangle = float*(__fastcall*)(void*, float*, const float*, const float*, const unsigned*);
using Inverse = float*(__fastcall*)(void*, float*, const void*, const float*);
Transform originalTransform = nullptr;
Rectangle originalRectangle = nullptr;
Inverse originalInverse = nullptr;
Log* logger = nullptr;
volatile LONG reports = 0;
void Missing() {
    if (logger && InterlockedIncrement(&reports) <= 8)
        logger->Write("china", "Map cartography is unavailable; using the retained finite UI fallback");
}
float* Center(void* floor, float* output) {
    Missing();
    const auto* bytes = static_cast<const uint8_t*>(floor);
    output[0] = static_cast<float>(*reinterpret_cast<const unsigned*>(bytes + 0x10)) * 0.5f;
    output[1] = static_cast<float>(*reinterpret_cast<const unsigned*>(bytes + 0x14)) * 0.5f;
    return output;
}
float* __fastcall Forward(void* manager, float* output, const float* point, const float* layout) {
    if (CartographyUnavailable(layout)) {
        void* floor = *reinterpret_cast<void**>(static_cast<uint8_t*>(manager) + 0x58);
        if (floor)
            return Center(floor, output);
    }
    return originalTransform(manager, output, point, layout);
}
float* __fastcall ForwardRectangle(void* manager, float* output, const float* point, const float* layout,
                                   const unsigned* rectangle) {
    if (CartographyUnavailable(layout)) {
        void* floor = *reinterpret_cast<void**>(static_cast<uint8_t*>(manager) + 0x58);
        if (floor)
            return Center(floor, output);
    }
    return originalRectangle(manager, output, point, layout, rectangle);
}
float* __fastcall Reverse(void* manager, float* output, const void* point, const float* layout) {
    const float* selected = layout ? layout : glide::ReadRuntimeField<const float*>(manager, 0x50);
    if (CartographyUnavailable(selected)) {
        Missing();
        CartographyOrigin(selected, output);
        return output;
    }
    return originalInverse(manager, output, point, layout);
}
} // namespace
const std::vector<HookSpec>& CartographyHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = CartographySymbols();
        return std::vector<HookSpec>{{symbols[0].name, symbols[0].symbol, reinterpret_cast<void*>(Forward),
                                      reinterpret_cast<void**>(&originalTransform), true},
                                     {symbols[1].name, symbols[1].symbol, reinterpret_cast<void*>(ForwardRectangle),
                                      reinterpret_cast<void**>(&originalRectangle), true},
                                     {symbols[2].name, symbols[2].symbol, reinterpret_cast<void*>(Reverse),
                                      reinterpret_cast<void**>(&originalInverse), true}};
    }();
    return hooks;
}
bool PrepareCartography(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "China map transforms require the original x64 executable";
#ifdef _WIN64
    if (runtime.config.build != 167409 || image.Machine() != Bitness::X64 || !module)
        return false;
    for (const auto& symbol : CartographySymbols())
        if (ResolveSymbol(image, symbol.symbol).count != 1) {
            error = std::string("China cartography pattern is not unique: ") + symbol.name;
            return false;
        }
    logger = &runtime.log;
    InterlockedExchange(&reports, 0);
    error.clear();
    return true;
#else
    (void)runtime;
    (void)image;
    (void)module;
    return false;
#endif
}
} // namespace china167409
} // namespace glide
