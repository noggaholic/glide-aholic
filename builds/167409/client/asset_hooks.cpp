#include "asset_hooks.h"
#include "asset_symbols.h"
#include "assets.h"
#include <Windows.h>
#include <cstring>
#include <cwchar>
namespace glide {
namespace china167409 {
namespace {
using ExeOpen = void*(__fastcall*)(const wchar_t*, unsigned, unsigned, unsigned*, void*);
using Info = int(__fastcall*)(const void*, unsigned*, void*);
using LocalFind = int(__fastcall*)(unsigned, unsigned char, unsigned*, unsigned*, unsigned*);
using MainFind = int(__fastcall*)(unsigned, unsigned*, unsigned*, unsigned*, unsigned*);
using Variant = int(__fastcall*)(unsigned, unsigned char, unsigned*);
using Route = void*(__fastcall*)(int);
using MapTask = void(__fastcall*)(void*);
ExeOpen originalOpen = nullptr;
Info originalInfo = nullptr;
LocalFind originalLocal = nullptr;
MainFind originalMain = nullptr;
Variant originalVariant = nullptr;
Route originalRoute = nullptr;
MapTask originalMapTask = nullptr;
void* priority = nullptr;
void** mainProvider = nullptr;
SourceAssets source;
std::wstring originalPath;
uint32_t Reference(const void* value) {
    if (!value)
        return 0;
    const auto* bytes = static_cast<const uint8_t*>(value);
    // The versionless terminator can be immediately followed by another native reference.
    const size_t size = bytes[4] || bytes[5] ? 8 : 6;
    return AssetReferenceId(ByteView(bytes, size));
}
void* __fastcall OpenSource(const wchar_t* path, unsigned flags, unsigned mode, unsigned* error, void* size) {
    const size_t count = originalPath.size();
    if (path && flags == 0x23 && mode == 3 && !_wcsnicmp(path, originalPath.c_str(), count) && path[count] == L'\\' &&
        std::wcsstr(path + count + 1, L"-1-100\\") && std::wcslen(path) >= 4 &&
        !_wcsicmp(path + std::wcslen(path) - 4, L".dat"))
        return originalOpen(path, 0x2d, 2, error, size);
    return originalOpen(path, flags, mode, error, size);
}
int __fastcall AssetInfo(const void* reference, unsigned* size, void* flags) {
    const uint32_t id = Reference(reference);
    uint32_t decoded = 0;
    const uint8_t audioReference[] = {0x49, 0x0c, 0x09, 0x01, 0, 0, 0, 0};
    if (id == 590410 && size && !std::memcmp(reference, audioReference, sizeof(audioReference))) {
        *size = 962;
        if (flags)
            *static_cast<unsigned*>(flags) = 0;
        return 1;
    }
    if (source.Ready() && source.Size(id, decoded)) {
        if (size)
            *size = decoded;
        if (flags)
            *static_cast<unsigned*>(flags) = 1;
        return 1;
    }
    return originalInfo(reference, size, flags);
}
int __fastcall LocalManifest(unsigned id, unsigned char stream, unsigned* physical, unsigned* size, unsigned* cached) {
    const int result = originalLocal(id, stream, physical, size, cached);
    uint32_t decoded = 0;
    if (result || stream || !source.Ready() || !source.Size(id, decoded))
        return result;
    if (physical)
        *physical = id;
    if (size)
        *size = decoded;
    if (cached)
        *cached = 1;
    return 1;
}
int __fastcall MainManifest(unsigned id, unsigned* physical, unsigned* metadata, unsigned* size, unsigned* cached) {
    const int result = originalMain(id, physical, metadata, size, cached);
    uint32_t decoded = 0;
    if (result || !source.Ready() || !mainProvider || !*mainProvider || !source.Size(id, decoded))
        return result;
    if (physical)
        *physical = id;
    if (metadata)
        *metadata = 0;
    if (size)
        *size = decoded;
    if (cached)
        *cached = 1;
    return 1;
}
int __fastcall FullTexture(unsigned id, unsigned char mode, unsigned* resolved) {
    const int result = originalVariant(id, mode, resolved);
    if (result || mode != 1 || !resolved || !source.Ready())
        return result;
    const uint32_t full = source.TextureVariant(id);
    if (!full)
        return result;
    *resolved = full;
    return 1;
}
void* __fastcall OriginalArchive(int id) {
    void* selected = originalRoute(id);
    if (id == 918051 || id == 195990 || id == 292090 || id == 195991 ||
        (source.Ready() && id > 0 && source.Contains(static_cast<unsigned>(id))))
        return nullptr;
    return selected;
}
void __fastcall InstalledMapTask(void* context) {
    auto* bytes = static_cast<uint8_t*>(context);
    void* service = *reinterpret_cast<void**>(bytes + 0x10);
    void** table = *static_cast<void***>(service);
    if (!*reinterpret_cast<unsigned*>(bytes + 0x18) && table[0x70 / sizeof(void*)] == priority) {
        const unsigned state = *reinterpret_cast<unsigned*>(static_cast<uint8_t*>(service) + 0xc8);
        if (state != 4 && state != 5) {
            reinterpret_cast<MapTask>(table[0x88 / sizeof(void*)])(service);
            return;
        }
    }
    originalMapTask(context);
}
} // namespace
void SourceContentReady() {
    source.ContentReady();
}
const std::vector<HookSpec>& AssetHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = AssetSymbols();
        std::vector<HookSpec> result;
        void* detours[] = {reinterpret_cast<void*>(OpenSource),      reinterpret_cast<void*>(AssetInfo),
                           reinterpret_cast<void*>(LocalManifest),   reinterpret_cast<void*>(MainManifest),
                           reinterpret_cast<void*>(FullTexture),     reinterpret_cast<void*>(OriginalArchive),
                           reinterpret_cast<void*>(InstalledMapTask)};
        void** originals[] = {reinterpret_cast<void**>(&originalOpen),    reinterpret_cast<void**>(&originalInfo),
                              reinterpret_cast<void**>(&originalLocal),   reinterpret_cast<void**>(&originalMain),
                              reinterpret_cast<void**>(&originalVariant), reinterpret_cast<void**>(&originalRoute),
                              reinterpret_cast<void**>(&originalMapTask)};
        for (size_t i = 0; i < sizeof(detours) / sizeof(detours[0]); ++i)
            result.push_back({symbols[i].name, symbols[i].symbol, detours[i], originals[i], true});
        return result;
    }();
    return hooks;
}
bool PrepareAssets(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "China original asset routing requires the original x64 executable";
#ifdef _WIN64
    if (runtime.config.build != 167409 || image.Machine() != Bitness::X64 || !module)
        return false;
    const auto& symbols = AssetSymbols();
    std::vector<uint32_t> addresses;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("China asset pattern is not unique: ") + symbol.name;
            return false;
        }
        addresses.push_back(match.rva);
    }
    if (!source.Load(runtime.config.archivePath, error))
        return false;
    originalPath = runtime.config.archivePath;
    priority = static_cast<uint8_t*>(module) + addresses[7];
    if (originalPath.empty() || originalPath.size() >= MAX_PATH) {
        error = "China native source archive path is too long";
        return false;
    }
    const auto* instruction = static_cast<const uint8_t*>(module) + addresses[8];
    int32_t displacement = 0;
    std::memcpy(&displacement, instruction + 3, sizeof(displacement));
    mainProvider = reinterpret_cast<void**>(const_cast<uint8_t*>(instruction + 7 + displacement));
    MEMORY_BASIC_INFORMATION memory = {};
    if (VirtualQuery(mainProvider, &memory, sizeof(memory)) != sizeof(memory) || memory.AllocationBase != module ||
        memory.State != MEM_COMMIT || (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        (reinterpret_cast<uintptr_t>(mainProvider) % alignof(void*))) {
        mainProvider = nullptr;
        error = "China native main-provider reference is not a readable aligned image slot";
        return false;
    }
    runtime.log.Write("china", "Original source asset index ready: " + std::to_string(source.MappingCount()));
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
