#include "startup.h"
#include "assets.h"
#include "content.h"
#include "native_resources.h"
#include "../../../client/src/runtime.h"
#include "../../../client/src/instance_hooks.h"
#include "glide/files.h"
#include "glide/local_archive.h"
#include "glide/tls.h"
#include <Windows.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <memory>
namespace glide {
namespace {
july14812::Assets assets;
july14812::Content content;
july14812::Resources resources;
std::unique_ptr<july14812::NativeResources> nativeResources;
std::wstring localPath;
void** mainArchive = nullptr;
void* localArchive = nullptr;
void* volatile pendingCoordinator = nullptr;
bool registered = false, prepared = false;
Log* log = nullptr;
std::atomic<unsigned> missingDocumentReports{0};
using PrepareArchive = int(__cdecl*)();
using RegisterAssets = void(__fastcall*)(unsigned, void*, void*);
using Coordinator = void(__fastcall*)(uint32_t*, void*);
using OpenLocal = void*(__fastcall*)(unsigned, const wchar_t*);
using OpenMain = void*(__fastcall*)(unsigned, void*);
using OpenFile = void*(__fastcall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned*);
using ContentLoad = void(__fastcall*)(unsigned*, void*, unsigned, const void*);
using ManifestRead = int(__fastcall*)(void*, void*, unsigned*, void*);
using ManifestQuery = unsigned(__fastcall*)(void*, void*, const wchar_t*);
using ByteAssign = void(__thiscall*)(void*, const void*, unsigned);
using ServerHello = unsigned(__fastcall*)(uint8_t*, void*, void*);
PrepareArchive prepareArchive = nullptr;
RegisterAssets registerAssets = nullptr;
Coordinator finish = nullptr, advance = nullptr, originalTick = nullptr;
OpenLocal openLocal = nullptr;
OpenMain openMain = nullptr;
OpenFile openFile = nullptr;
ContentLoad contentLoad = nullptr;
ManifestRead manifestRead = nullptr;
ManifestQuery manifestQuery = nullptr;
ByteAssign byteAssign = nullptr;
ServerHello serverHello = nullptr;
void *oldLookup = nullptr, *oldStageZero = nullptr, *oldStageTwo = nullptr, *oldCipher = nullptr;
void* __fastcall Local(unsigned flags, const wchar_t*) {
    void* archive = openLocal(flags & ~8u, localPath.c_str());
    if (archive) {
        std::string error;
        if (!nativeResources->Seed(resources, archive, error)) {
            log->Write("july-startup", error);
            ExitProcess(3);
        }
    }
    localArchive = archive;
    return archive;
}
void* __fastcall Main(unsigned flags, void* unused) {
    return openMain(flags | 8u, unused);
}
void* __fastcall File(void* archive, unsigned id, unsigned revision, unsigned flags, unsigned callback,
                      unsigned context, unsigned extra, unsigned* error) {
    void* main = mainArchive ? *mainArchive : nullptr;
    void* native = main ? static_cast<void**>(main)[2] : nullptr;
    if ((!archive || archive == native) && id && id <= 0xfff && localArchive)
        archive = static_cast<void**>(localArchive)[2];
    return openFile(archive, id, revision, flags, callback, context, extra, error);
}
void __fastcall Load(unsigned* result, void* unused, unsigned size, const void* seed) {
    std::string error;
    if (!content.Prepare(ByteView(seed, size), error)) {
        log->Write("july-startup", error);
        ExitProcess(3);
    }
    contentLoad(result, unused, size, seed);
}
int __fastcall Manifest(void* reader, void* unused, unsigned*, void* cipher) {
    const auto bytes = content.Document(nullptr);
    if (!bytes.size)
        return 0;
    unsigned descriptor[] = {0, static_cast<unsigned>(bytes.size),
                             static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data))};
    return manifestRead(reader, unused, descriptor, cipher);
}
int __stdcall Lookup(void*, const void* name, unsigned, void*, unsigned* descriptor) {
    if (!descriptor)
        return 0;
    descriptor[0] = descriptor[1] = descriptor[2] = 0;
    const wchar_t* requested = name ? *static_cast<const wchar_t* const*>(name) : nullptr;
    if (!requested)
        return 0;
    const auto bytes = content.Document(requested);
    if (!bytes.size) {
        if (log && missingDocumentReports.fetch_add(1, std::memory_order_relaxed) < 32) {
            std::string text;
            if (WideToUtf8(requested, text))
                log->Write("july-content", "Missing requested document: " + text);
        }
        return 0;
    }
    descriptor[1] = static_cast<unsigned>(bytes.size);
    descriptor[2] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data));
    return 1;
}
unsigned __fastcall OptionalManifest(void* catalog, void* unused, const wchar_t* name) {
    // Installed July DAT has no optional patch/download manifest. Preserve the
    // complete native query whenever its catalog is present.
    if (catalog && !static_cast<void**>(catalog)[1] && name)
        return 0;
    return manifestQuery(catalog, unused, name);
}
void __fastcall InstalledStage(uint32_t* coordinator, void*) {
    if (!coordinator || coordinator[0x40 / 4] || coordinator[0x44 / 4] || coordinator[0x48 / 4] ||
        (coordinator[6] != 0 && coordinator[6] != 2) || pendingCoordinator) {
        log->Write("july-startup", "Occupied or invalid native startup coordinator");
        ExitProcess(3);
    }
    if (coordinator[6] == 0) {
        // Native stage1 initializes preferences and opens the DAT; retain it.
        advance(coordinator, nullptr);
        return;
    }
    bool opened = coordinator[0] || prepareArchive();
    void* archive = mainArchive ? *mainArchive : nullptr;
    void* provider = assets.NativeManifest();
    if (opened && archive && provider) {
        if (!registered) {
            registerAssets(0, provider, archive);
            registered = true;
        }
    } else
        opened = false;
    if (opened) {
        std::string error;
        opened = nativeResources->ProbeAndPublish(resources, archive, true, error);
        if (!opened)
            log->Write("july-startup", error);
    }
    coordinator[0] = opened ? 1u : 0u;
    coordinator[1] = opened ? 0u : 1u;
    // Start schedules its tick after this callback. Finish on that first tick.
    InterlockedExchangePointer(&pendingCoordinator, coordinator);
}
void __fastcall Tick(uint32_t* coordinator, void* unused) {
    if (InterlockedCompareExchangePointer(&pendingCoordinator, nullptr, coordinator) == coordinator) {
        finish(coordinator, unused);
        return; // Native finish may free coordinator.
    }
    originalTick(coordinator, unused);
}
unsigned __fastcall Hello(uint8_t* client, void* unused, void* reader) {
    const auto session = LocalTlsSessionId(), master = LocalTlsMasterSecret();
    byteAssign(client + 0xde48, session.data, static_cast<unsigned>(session.size));
    std::memcpy(client + 0x64, master.data, master.size);
    return serverHello(client, unused, reader);
}
void __fastcall Cipher(uint8_t* state, void*, unsigned, const void*) {
    const uint8_t key[] = "GLIDE_AHOLIC_RC4_KEY";
    std::memset(state, 0, 8);
    for (unsigned i = 0; i < 256; ++i)
        state[8 + i] = static_cast<uint8_t>(i);
    unsigned j = 0;
    for (unsigned i = 0; i < 256; ++i) {
        j = (j + state[8 + i] + key[i % (sizeof(key) - 1)]) & 255;
        std::swap(state[8 + i], state[8 + j]);
    }
}
bool CacheFile(const std::wstring& directory, std::string& error) {
    return CreateLocalArchive(directory + L"/client/empty-cache.bin", localPath, error);
}
} // namespace
const std::vector<JulySymbol>& JulyStartupSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<JulySymbol> symbols = {SYMBOL(build),
                                                    SYMBOL(prepare),
                                                    SYMBOL(registerAssets),
                                                    SYMBOL(publisher),
                                                    SYMBOL(finish),
                                                    SYMBOL(advance),
                                                    SYMBOL(byteAssign),
                                                    SYMBOL(localArchive),
                                                    SYMBOL(mainArchive),
                                                    SYMBOL(archiveOpen),
                                                    SYMBOL(contentLoad),
                                                    SYMBOL(manifestReader),
                                                    SYMBOL(recordLookup),
                                                    SYMBOL(stageZero),
                                                    SYMBOL(stageTwo),
                                                    SYMBOL(tick),
                                                    SYMBOL(installedManifestQuery),
                                                    SYMBOL(tlsHello),
                                                    SYMBOL(msgconnKey)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& JulyStartupHooks() {
    static const std::vector<HookSpec> hooks = [] {
        const auto& symbols = JulyStartupSymbols();
        void* detours[] = {reinterpret_cast<void*>(Local),          reinterpret_cast<void*>(Main),
                           reinterpret_cast<void*>(File),           reinterpret_cast<void*>(Load),
                           reinterpret_cast<void*>(Manifest),       reinterpret_cast<void*>(Lookup),
                           reinterpret_cast<void*>(InstalledStage), reinterpret_cast<void*>(InstalledStage),
                           reinterpret_cast<void*>(Tick),           reinterpret_cast<void*>(OptionalManifest),
                           reinterpret_cast<void*>(Hello),          reinterpret_cast<void*>(Cipher)};
        void** originals[] = {reinterpret_cast<void**>(&openLocal),
                              reinterpret_cast<void**>(&openMain),
                              reinterpret_cast<void**>(&openFile),
                              reinterpret_cast<void**>(&contentLoad),
                              reinterpret_cast<void**>(&manifestRead),
                              &oldLookup,
                              &oldStageZero,
                              &oldStageTwo,
                              reinterpret_cast<void**>(&originalTick),
                              reinterpret_cast<void**>(&manifestQuery),
                              reinterpret_cast<void**>(&serverHello),
                              &oldCipher};
        std::vector<HookSpec> result;
        for (size_t i = 7; i < symbols.size(); ++i)
            result.push_back({symbols[i].name, symbols[i].symbol, detours[i - 7], originals[i - 7], true});
        return result;
    }();
    return hooks;
}
bool PrepareJulyStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "July startup requires the original fixed-base build14812 x86 client";
    if (prepared || reinterpret_cast<uintptr_t>(module) != 0x400000u || sizeof(void*) != 4 ||
        image.Machine() != Bitness::X86 || runtime.config.build != 14812)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : JulyStartupSymbols()) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("July startup RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(match.rva);
    }
    july14812::ResourceApi api;
    if (!july14812::BindResourceApi(image, module, api, error))
        return false;
    auto base = static_cast<uint8_t*>(module);
    uint32_t slot;
    std::memcpy(&slot, base + offsets[3] + 12, 4);
    MEMORY_BASIC_INFORMATION region{};
    error = "July archive singleton is outside writable image data";
    if (!VirtualQuery(reinterpret_cast<void*>(uintptr_t(slot)), &region, sizeof(region)) ||
        region.AllocationBase != module || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return false;
    if (!content.Load(runtime.config.dataDirectory + L"/content", error) ||
        !assets.Open(runtime.config.archivePath, error) ||
        !resources.Load(runtime.config.dataDirectory + L"/content/resources", error))
        return false;
    localPath = runtime.config.runtimeDirectory + L"/ResourceLocal.dat";
    if (localPath.size() >= MAX_PATH || !CacheFile(runtime.config.dataDirectory, error))
        return false;
    if (!InstallInstanceHooks(runtime.hooks, runtime.config.build, runtime.config.slot, error))
        return false;
    prepareArchive = reinterpret_cast<PrepareArchive>(base + offsets[1]);
    registerAssets = reinterpret_cast<RegisterAssets>(base + offsets[2]);
    finish = reinterpret_cast<Coordinator>(base + offsets[4]);
    advance = reinterpret_cast<Coordinator>(base + offsets[5]);
    byteAssign = reinterpret_cast<ByteAssign>(base + offsets[6]);
    mainArchive = reinterpret_cast<void**>(uintptr_t(slot));
    nativeResources.reset(new july14812::NativeResources(api));
    log = &runtime.log;
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
