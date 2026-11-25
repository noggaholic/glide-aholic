#include "startup.h"
#include "content.h"
#include "../../../client/src/runtime.h"
#include <Windows.h>
#include <cstring>

namespace glide {
namespace {
hot49284::Content content;
using Stage = void(__fastcall*)(void*, void*);
using Tick = void(__fastcall*)(void*, void*, float);
using Allocate = void*(__fastcall*)(unsigned, unsigned);
using Packfile = void*(__fastcall*)(unsigned, unsigned, void*);
Allocate allocateRoot = nullptr;
Packfile createPackfile = nullptr;
Stage publishRoot = nullptr, waitForLogin = nullptr, originalStage = nullptr;
Tick originalTick = nullptr;
using LoginState = void(__fastcall*)(unsigned, void*);
LoginState setLoginState = nullptr;
void (*loginReady)() = nullptr;
Glide49284PublishAssetFn publishAsset = nullptr;
Glide49284PublishPropertyFn publishProperty = nullptr;
Glide49284QueryAssetFn queryAsset = nullptr;
Log* log = nullptr;
void* pendingCoordinator = nullptr;
LONG pending = 0;
bool published = false, prepared = false;
bool PublishRoot() {
    if (published)
        return false;
    const ByteView root = content.Root();
    void* bytes = allocateRoot(static_cast<unsigned>(root.size), 0);
    if (!bytes)
        return false;
    std::memcpy(bytes, root.data, root.size);
    // Native factory takes ownership of this native-heap allocation (mode 2).
    void* object = createPackfile(2, static_cast<unsigned>(root.size), bytes);
    if (!object)
        return false;
    publishRoot(object, nullptr);
    void** table = *static_cast<void***>(object);
    reinterpret_cast<Stage>(table[0])(object, nullptr);
    published = true;
    return true;
}
void __fastcall ManifestStage(void* coordinator, void* unused) {
    if (!glide_49284_offline_stage_clean(static_cast<const uint32_t*>(coordinator)) || pendingCoordinator || pending ||
        !PublishRoot()) {
        log->Write("hot-startup", "Native metadata stage rejected");
        originalStage(coordinator, unused);
        return;
    }
    // Preserve the native per-manifest publish/query order.
    for (size_t i = 0; i < 5; ++i) {
        const ByteView bytes = content.Manifest(i);
        Glide49284InstalledManifest manifest;
        if (!glide_49284_installed_manifest(bytes.data, bytes.size, &manifest)) {
            originalStage(coordinator, unused);
            return;
        }
        glide_49284_publish_manifest(&manifest, publishAsset, publishProperty);
        for (uint32_t n = 0; n < manifest.records.count; ++n) {
            const uint8_t* record = bytes.data + manifest.records.start + size_t(n) * 16;
            uint32_t revision = 0, flags = 0, size = 0, cached = 0;
            if (!queryAsset(glide_49284_word(record), &revision, &flags, &size, &cached) ||
                revision != glide_49284_word(record + 4) || flags != glide_49284_word(record + 12) ||
                size != glide_49284_word(record + 8) || cached) {
                log->Write("hot-startup", "Native metadata registry verification failed");
                originalStage(coordinator, unused);
                return;
            }
        }
    }
    pendingCoordinator = coordinator;
    InterlockedExchange(&pending, 1);
    log->Write("hot-startup", "Original metadata published; login initialization waits for native tick");
}
void __fastcall NativeTick(void* coordinator, void* unused, float delta) {
    if (coordinator == pendingCoordinator) {
        if (InterlockedCompareExchange(&pending, 0, 1) == 1) {
            if (!glide_49284_offline_stage_clean(static_cast<const uint32_t*>(coordinator))) {
                pendingCoordinator = nullptr;
                originalStage(coordinator, unused);
                originalTick(coordinator, unused, delta);
                return;
            }
            // All profile preparation (including pinned private browser files and
            // local URL) succeeded before hooks were enabled. No updater download
            // is needed for that supplied package; retain native login and cancel.
            log->Write("hot-startup", "Starting native login with validated local browser package");
            setLoginState(6, nullptr);
            loginReady();
            log->Write("hot-startup", "Waiting for native login or cancellation");
        }
        waitForLogin(coordinator, nullptr);
        // Native wait may complete/free the coordinator. Never dereference it
        // after this call. Its native owner also stops scheduling its ticks.
        return;
    }
    originalTick(coordinator, unused, delta);
}
} // namespace
const std::vector<HotSymbol>& HotStartupSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(hot_root_allocate),
                                                   SYMBOL(hot_root_packfile),
                                                   SYMBOL(hot_root_publishRoot),
                                                   SYMBOL(client_49284_publish_asset),
                                                   SYMBOL(client_49284_publish_property),
                                                   SYMBOL(client_49284_query_asset),
                                                   SYMBOL(client_49284_login_wait),
                                                   SYMBOL(client_49284_manifest_stage),
                                                   SYMBOL(client_49284_patcher_tick),
                                                   SYMBOL(client_49284_login_state),
                                                   SYMBOL(client_49284_login_ready)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotStartupHooks() {
    static const std::vector<HookSpec> hooks = {
        {HotStartupSymbols()[7].name, HotStartupSymbols()[7].symbol, reinterpret_cast<void*>(ManifestStage),
         reinterpret_cast<void**>(&originalStage), true},
        {HotStartupSymbols()[8].name, HotStartupSymbols()[8].symbol, reinterpret_cast<void*>(NativeTick),
         reinterpret_cast<void**>(&originalTick), true}};
    return hooks;
}
bool PrepareHotStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT startup requires an unprepared build 49284 x86 client";
    if (prepared || !module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 || runtime.config.build != 49284)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : HotStartupSymbols()) {
        const auto found = ResolveSymbol(image, symbol.symbol);
        if (found.count != 1) {
            error = std::string("HoT startup RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(found.rva);
    }
    if (!content.Load(runtime.config.dataDirectory + L"/content", error))
        return false;
    // Parse all manifests before installing or executing any native callback.
    for (size_t i = 0; i < 5; ++i) {
        const ByteView bytes = content.Manifest(i);
        Glide49284InstalledManifest manifest;
        if (!glide_49284_installed_manifest(bytes.data, bytes.size, &manifest)) {
            error = "Invalid original HoT manifest";
            return false;
        }
    }
    uint8_t* base = static_cast<uint8_t*>(module);
    allocateRoot = reinterpret_cast<Allocate>(base + offsets[0]);
    createPackfile = reinterpret_cast<Packfile>(base + offsets[1]);
    publishRoot = reinterpret_cast<Stage>(base + offsets[2]);
    publishAsset = reinterpret_cast<Glide49284PublishAssetFn>(base + offsets[3]);
    publishProperty = reinterpret_cast<Glide49284PublishPropertyFn>(base + offsets[4]);
    queryAsset = reinterpret_cast<Glide49284QueryAssetFn>(base + offsets[5]);
    waitForLogin = reinterpret_cast<Stage>(base + offsets[6]);
    setLoginState = reinterpret_cast<LoginState>(base + offsets[9]);
    loginReady = reinterpret_cast<void (*)()>(base + offsets[10]);
    log = &runtime.log;
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
