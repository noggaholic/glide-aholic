#include "startup.h"
#include "assets.h"
#include "content.h"
#include "../../../client/src/runtime.h"
#include "../../../client/src/instance_hooks.h"
#include "glide/files.h"
#include "glide/local_archive.h"
#include <Windows.h>
#include <cstring>
#include <algorithm>

namespace glide {
namespace {
JuneAssets assets;
JuneContent content;
JuneStartupInputs inputs;
std::wstring localPath;
void** mainArchive = nullptr;
void* localArchive = nullptr;
bool published = false;
LONG videoMemory = 0;
using PrepareArchive = int(__cdecl*)();
using RegisterAssets = void(__fastcall*)(unsigned, void*, void*);
using OpenLocal = void*(__fastcall*)(unsigned, const wchar_t*);
using OpenMain = void*(__fastcall*)(unsigned, void*);
using OpenFile = void*(__fastcall*)(void*, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned, unsigned*);
using ContentLoad = void(__fastcall*)(unsigned*, void*, unsigned, const void*);
using ManifestRead = int(__fastcall*)(void*, void*, unsigned*, void*);
using DeviceCreate = int(__fastcall*)(void*, void*, void*, unsigned, unsigned);
using Capability = unsigned(__fastcall*)(unsigned, void*);
PrepareArchive prepareArchive = nullptr;
RegisterAssets registerAssets = nullptr;
OpenLocal openLocal = nullptr;
OpenMain openMain = nullptr;
OpenFile openFile = nullptr;
ContentLoad contentLoad = nullptr;
ManifestRead manifestRead = nullptr;
DeviceCreate deviceCreate = nullptr;
Capability capability = nullptr;
void* oldLookup = nullptr;
void* oldUpdate = nullptr;
void* oldCipher = nullptr;
void* __fastcall Local(unsigned flags, const wchar_t*) {
    localArchive = openLocal(flags & ~8u, localPath.c_str());
    return localArchive;
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
    if (!inputs.prepare || !inputs.prepare(inputs.documents, ByteView(seed, size), error)) {
        TerminateProcess(GetCurrentProcess(), ERROR_INVALID_DATA);
        return;
    }
    contentLoad(result, unused, size, seed);
}
int __fastcall Manifest(void* reader, void* unused, unsigned*, void* cipher) {
    const auto bytes = inputs.document ? inputs.document(inputs.documents, nullptr) : ByteView();
    if (!bytes.size || sizeof(void*) != 4)
        return 0;
    unsigned descriptor[] = {0, static_cast<unsigned>(bytes.size),
                             static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data))};
    return manifestRead(reader, unused, descriptor, cipher);
}
int __stdcall Lookup(void*, const void* name, unsigned, void*, unsigned* descriptor) {
    if (!descriptor)
        return 0;
    descriptor[0] = descriptor[1] = descriptor[2] = 0;
    if (!name || sizeof(void*) != 4)
        return 0;
    const auto bytes =
        inputs.document ? inputs.document(inputs.documents, *static_cast<const wchar_t* const*>(name)) : ByteView();
    if (!bytes.size)
        return 0;
    descriptor[1] = static_cast<unsigned>(bytes.size);
    descriptor[2] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data));
    return 1;
}
void __fastcall Update(unsigned* coordinator, void*) {
    bool opened = prepareArchive() != 0;
    if (opened && !published) {
        void* provider = inputs.assets ? inputs.assets->NativeManifest() : nullptr;
        opened = provider && mainArchive && *mainArchive;
        if (opened) {
            registerAssets(0, provider, *mainArchive);
            published = true;
        }
    }
    coordinator[0] = opened ? 1u : 0u;
    coordinator[1] = opened ? 0u : 1u;
    coordinator[0x100 / 4] = opened ? 6u : 7u; // Let the native next tick complete startup.
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
int __fastcall Device(void* d3d, void* unused, void* state, unsigned behavior, unsigned presentation) {
    const int result = deviceCreate(d3d, unused, state, behavior, presentation);
    if (result >= 0 && state) {
        void* device = *reinterpret_cast<void**>(static_cast<uint8_t*>(state) + 0xa0);
        if (device) {
            using Available = unsigned(__stdcall*)(void*);
            const unsigned memory = reinterpret_cast<Available>((*static_cast<void***>(device))[4])(device);
            InterlockedExchange(&videoMemory, static_cast<LONG>(memory));
        }
    }
    return result;
}
unsigned __fastcall Cap(unsigned id, void* unused) {
    const unsigned result = capability(id, unused);
    return id == 30 && !result ? static_cast<unsigned>(InterlockedCompareExchange(&videoMemory, 0, 0)) : result;
}
bool CacheFile(const std::wstring& directory, std::string& error) {
    return CreateLocalArchive(directory + L"/client/empty-cache.bin", localPath, error);
}
} // namespace
const std::vector<JuneSymbol>& JuneStartupSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<JuneSymbol> symbols = {
        SYMBOL(beta_prepare_archive),      SYMBOL(client_2012_register_assets), SYMBOL(beta_archive_publisher),
        SYMBOL(client_2012_file_open),     SYMBOL(beta_open_local_dat),         SYMBOL(client_2012_main_archive),
        SYMBOL(client_2012_archive_open),  SYMBOL(client_2012_content_load),    SYMBOL(client_2012_manifest_reader),
        SYMBOL(client_2012_record_lookup), SYMBOL(client_2012_update_check),    SYMBOL(beta_msgconn_rc4),
        SYMBOL(beta_dx9_create_device),    SYMBOL(client_2012_capability)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& JuneStartupHooks() {
    static const std::vector<HookSpec> hooks = MakeJuneStartupHooks(JuneStartupSymbols());
    return hooks;
}
std::vector<HookSpec> MakeJuneStartupHooks(const std::vector<JuneSymbol>& symbols) {
    if (symbols.size() != 14)
        return {};
    void* detours[] = {reinterpret_cast<void*>(Local),    reinterpret_cast<void*>(Main),
                       reinterpret_cast<void*>(File),     reinterpret_cast<void*>(Load),
                       reinterpret_cast<void*>(Manifest), reinterpret_cast<void*>(Lookup),
                       reinterpret_cast<void*>(Update),   reinterpret_cast<void*>(Cipher),
                       reinterpret_cast<void*>(Device),   reinterpret_cast<void*>(Cap)};
    void** originals[] = {reinterpret_cast<void**>(&openLocal),
                          reinterpret_cast<void**>(&openMain),
                          reinterpret_cast<void**>(&openFile),
                          reinterpret_cast<void**>(&contentLoad),
                          reinterpret_cast<void**>(&manifestRead),
                          &oldLookup,
                          &oldUpdate,
                          &oldCipher,
                          reinterpret_cast<void**>(&deviceCreate),
                          reinterpret_cast<void**>(&capability)};
    std::vector<HookSpec> result;
    for (size_t i = 4; i < symbols.size(); ++i)
        result.push_back({symbols[i].name, symbols[i].symbol, detours[i - 4], originals[i - 4], true});
    return result;
}
bool PrepareJuneStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    return PrepareJuneStartupInputs(runtime, image, module, error, JuneStartupSymbols(),
                                    MakeJuneStartupInputs(14505, assets, content));
}
bool PrepareJuneStartupInputs(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error,
                              const std::vector<JuneSymbol>& symbols, const JuneStartupInputs& selected) {
    error = "June startup requires matching 14422 or 14505 x86 executable and own inputs";
    if (!module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 ||
        (selected.build != 14422 && selected.build != 14505) || runtime.config.build != selected.build ||
        symbols.size() != 14 || !selected.assets || !selected.documents || !selected.load || !selected.prepare ||
        !selected.document)
        return false;
    const auto* build = BuildTable::Find(selected.build);
    if (!build || ResolveSymbol(image, build->buildGetter).count != 1)
        return false;
    std::vector<uint32_t> offsets;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("June startup RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets.push_back(match.rva);
    }
    auto* base = static_cast<uint8_t*>(module);
    int32_t displacement;
    std::memcpy(&displacement, base + offsets[3] + 23, 4);
    error = "June File_Open does not call the verified archive function";
    if (int64_t(offsets[3]) + 27 + displacement != offsets[6])
        return false;
    uint32_t slot;
    std::memcpy(&slot, base + offsets[2] + 12, 4);
    MEMORY_BASIC_INFORMATION region = {};
    error = "June archive singleton is outside writable image data";
    if (!VirtualQuery(reinterpret_cast<void*>(uintptr_t(slot)), &region, sizeof(region)) ||
        region.AllocationBase != module || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return false;
    if (!selected.load(selected.documents, runtime.config.dataDirectory + L"/content", error) ||
        !selected.assets->Open(runtime.config.archivePath, error))
        return false;
    wchar_t traceFlag[2] = {};
    if (GetEnvironmentVariableW(L"GLIDE_TRACE_ASSETS", traceFlag, 2) == 1 && traceFlag[0] == L'1') {
        selected.assets->TraceRequests(&runtime.log);
        runtime.log.Write("asset-request", "Optional installed-asset trace enabled; limit=8192 distinct requests");
    }
    localPath = runtime.config.runtimeDirectory + L"/Local.dat";
    if (localPath.size() >= MAX_PATH || !CacheFile(runtime.config.dataDirectory, error))
        return false;
    prepareArchive = reinterpret_cast<PrepareArchive>(base + offsets[0]);
    registerAssets = reinterpret_cast<RegisterAssets>(base + offsets[1]);
    mainArchive = reinterpret_cast<void**>(uintptr_t(slot));
    if (!InstallInstanceHooks(runtime.hooks, runtime.config.build, runtime.config.slot, error))
        return false;
    inputs = selected;
    published = false;
    error.clear();
    return true;
}
} // namespace glide
