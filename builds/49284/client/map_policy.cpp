#include "map_policy.h"
#include "../../../client/src/runtime.h"
#include <cstring>

namespace glide {
namespace {
hot49284::MapCatalog maps;
using Allocate = void*(__fastcall*)(unsigned, unsigned);
using Construct = void*(__fastcall*)(void*, void*, void**, unsigned);
using Download = void*(__fastcall*)(void*, void*, const void*);
using Method = void(__fastcall*)(void*, void*);
Allocate allocate = nullptr;
Construct construct = nullptr;
Download originalDownload = nullptr;
Glide49284QueryAssetFn query = nullptr;
void* originalPriority = nullptr;
bool prepared = false;
void __fastcall BackgroundPriority(void*, void*) {
    // Local installed-only mode has no background downloader to reprioritize.
}
void* __fastcall DownloadMap(void* manager, void* unused, const void* identifier) {
    std::array<uint8_t, 16> guid{};
    if (identifier)
        std::memcpy(guid.data(), identifier, guid.size());
    const auto* map = identifier ? maps.FindGuid(guid) : nullptr;
    if (!map)
        return originalDownload(manager, unused, identifier);
    if (!VerifyHotMapAssets(*map, query))
        return nullptr;
    void* object = allocate(24, 0);
    if (!object)
        return nullptr;
    object = construct(object, nullptr, nullptr, 0);
    if (!object)
        return nullptr;
    void** table = *static_cast<void***>(object);
    reinterpret_cast<Method>(table[1])(object, nullptr);
    return object;
}
} // namespace
bool VerifyHotMapAssets(const hot49284::MapGeometry& map, Glide49284QueryAssetFn lookup) {
    if (!lookup || !map.id || !map.assets[0].id)
        return false;
    for (const auto& asset : map.assets) {
        if (!asset.id)
            continue;
        uint32_t revision = 0, flags = 0, size = 0, cached = 0;
        if (!lookup(asset.id, &revision, &flags, &size, &cached) || revision != asset.revision || size != asset.size ||
            (flags & ~0x08000000u) != asset.flags || cached != ((flags >> 27) & 1u))
            return false;
    }
    return true;
}
const std::vector<HotSymbol>& HotMapPolicySymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(hot_root_allocate), SYMBOL(hot_root_downloadConstruct),
                                                   SYMBOL(client_49284_query_asset), SYMBOL(hot_root_mapDownload),
                                                   SYMBOL(hot_root_backgroundPatchPriority)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotMapPolicyHooks() {
    static const std::vector<HookSpec> hooks = {{HotMapPolicySymbols()[3].name, HotMapPolicySymbols()[3].symbol,
                                                 reinterpret_cast<void*>(DownloadMap),
                                                 reinterpret_cast<void**>(&originalDownload), true},
                                                {HotMapPolicySymbols()[4].name, HotMapPolicySymbols()[4].symbol,
                                                 reinterpret_cast<void*>(BackgroundPriority), &originalPriority, true}};
    return hooks;
}
bool PrepareHotMapPolicy(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT local map policy requires an unprepared build 49284 x86 client";
    if (prepared || !module || sizeof(void*) != 4 || runtime.config.build != 49284 || image.Machine() != Bitness::X86)
        return false;
    std::array<uint32_t, 5> offsets;
    for (size_t i = 0; i < offsets.size(); ++i) {
        const auto& symbol = HotMapPolicySymbols()[i];
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("HoT map policy RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets[i] = match.rva;
    }
    if (!maps.Load(runtime.config.dataDirectory + L"/maps.bin", error))
        return false;
    uint8_t* base = static_cast<uint8_t*>(module);
    allocate = reinterpret_cast<Allocate>(base + offsets[0]);
    construct = reinterpret_cast<Construct>(base + offsets[1]);
    query = reinterpret_cast<Glide49284QueryAssetFn>(base + offsets[2]);
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
