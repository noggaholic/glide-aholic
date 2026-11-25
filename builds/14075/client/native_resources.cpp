#include "native_resources.h"
#include "glide/files.h"
#include "glide/local_archive.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace glide {
namespace april14075 {
namespace {
NativeResourceInputs inputs;
void* localArchive = nullptr;
LONG joined = 0, deferred = 0, prepareCalls = 0;
LONG archiveOpened = 0, fileTableReady = 0;
using OpenLocalFn = void*(__fastcall*)(unsigned, void*);
using WriteFn = void(__fastcall*)(void*, void*, unsigned, unsigned, unsigned char, unsigned, unsigned, unsigned, void**,
                                  unsigned);
using ManifestInitFn = int(__fastcall*)(void*, unsigned);
using PrepareFn = int(__cdecl*)();
using DeriveFn = void(__fastcall*)(unsigned, const unsigned char*, unsigned char*);
using PacksFn = int(__fastcall*)(unsigned char*, void*, void*);
using ManifestFn = int(__fastcall*)(unsigned char*, void*, unsigned*, void*);
using AvailabilityFn = int(__fastcall*)(unsigned, void*, unsigned*);
using ResolveFn = int(__fastcall*)(unsigned, unsigned*);
using DecodeFn = unsigned(__fastcall*)(const unsigned short*, unsigned char*);
using JoinFn = void(__stdcall*)(const short*);
OpenLocalFn openLocal = nullptr;
WriteFn writeRaw = nullptr;
ManifestInitFn initializeManifest = nullptr;
PrepareFn prepareArchive = nullptr;
DeriveFn deriveKey = nullptr;
PacksFn openPacks = nullptr;
ManifestFn readManifest = nullptr;
AvailabilityFn availability = nullptr;
ResolveFn resolveReference = nullptr;
DecodeFn decodeReference = nullptr;
JoinFn joinResult = nullptr;
void* oldLookup = nullptr;
void* oldCipher = nullptr; // Hooks::Create requires storage even for full replacements.
bool Cache(const wchar_t* path) {
    std::string error;
    return CreateLocalArchive(inputs.emptyArchiveTemplate, path, error);
}
void* __fastcall Local(unsigned flags, void*) {
    const bool ready =
        inputs.cache ? inputs.cache(inputs.cacheContext, inputs.localPath.c_str()) : Cache(inputs.localPath.c_str());
    if (!ready)
        return nullptr;
    localArchive = openLocal(flags & ~8u, const_cast<wchar_t*>(inputs.localPath.c_str()));
    return localArchive;
}
void __fastcall Write(void* archive, void* unused, unsigned file, unsigned position, unsigned char option,
                      unsigned data, unsigned size, unsigned completion, void** operation, unsigned priority) {
    void* main = inputs.mainArchive ? *inputs.mainArchive : nullptr;
    if (archive == main && localArchive && localArchive != archive)
        archive = localArchive;
    writeRaw(archive, unused, file, position, option, data, size, completion, operation, priority);
}
int __fastcall Initialize(void* archive, unsigned limit) {
    void* main = inputs.mainArchive ? *inputs.mainArchive : nullptr;
    if (!main) {
        InterlockedExchange(&deferred, 1);
        if (archive == localArchive)
            localArchive = nullptr;
        return 1;
    }
    return initializeManifest(archive, limit);
}
int __cdecl Prepare() {
    if (InterlockedIncrement(&prepareCalls) == 1) {
        const int opened = prepareArchive();
        InterlockedExchange(&archiveOpened, opened);
        if (!opened)
            return 0;
        void* main = inputs.mainArchive ? *inputs.mainArchive : nullptr;
        if (InterlockedCompareExchange(&deferred, 0, 0)) {
            // Open() has already validated all 2,151 relationships. This is
            // the original file-table readiness check, not a manifest limit.
            if (!main || !initializeManifest || !inputs.membership.Has(103883))
                return 0;
            InterlockedExchange(&fileTableReady, 1);
            if (!initializeManifest(main, 0x0fff))
                return 0;
            InterlockedExchange(&deferred, 0);
        }
        return 0; // The native launcher consumes this first tick separately.
    }
    return static_cast<int>(InterlockedCompareExchange(&archiveOpened, 0, 0));
}
void __fastcall Cipher(unsigned char* state, void*, unsigned, const void*) {
    static const unsigned char key[] = "GLIDE_AHOLIC_RC4_KEY";
    static_assert(sizeof(key) - 1 == 20, "April MsgConn key length");
    std::memset(state, 0, 8);
    for (unsigned i = 0; i < 256; ++i)
        state[8 + i] = static_cast<unsigned char>(i);
    unsigned j = 0;
    for (unsigned i = 0; i < 256; ++i) {
        j = (j + state[8 + i] + key[i % (sizeof(key) - 1)]) & 255;
        std::swap(state[8 + i], state[8 + j]);
    }
}
void __fastcall Derive(unsigned size, const unsigned char* key, unsigned char* output) {
    deriveKey(size, key, output);
    if (!output)
        return;
    unsigned char* derived = nullptr;
    unsigned bytes = 0;
    std::memcpy(&derived, output + 4, sizeof(derived));
    std::memcpy(&bytes, output + 8, sizeof(bytes));
    if (derived && bytes >= 20)
        std::memcpy(derived, Content::Key().data, 20);
}
int __fastcall Packs(unsigned char* loader, void* unused, void* names) {
    const int result = openPacks(loader, unused, names);
    if (result && loader) {
        unsigned flags;
        std::memcpy(&flags, loader + 0x44, sizeof(flags));
        flags &= ~4u;
        std::memcpy(loader + 0x44, &flags, sizeof(flags));
    }
    return result;
}
int __fastcall Manifest(unsigned char* reader, void* unused, unsigned* descriptor, void* registry) {
    const ByteView bytes = inputs.document(inputs.documents, nullptr);
    unsigned replacement[3];
    if (bytes.data && bytes.size) {
        replacement[0] = 0;
        replacement[1] = static_cast<unsigned>(bytes.size);
        replacement[2] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data));
        descriptor = replacement;
    }
    return readManifest(reader, unused, descriptor, registry);
}
int __stdcall Lookup(void*, const void* name, unsigned, void*, unsigned* descriptor) {
    const auto* requested = name ? *static_cast<const wchar_t* const*>(name) : nullptr;
    if (!requested)
        return 0; // A nameless lookup is not the manifest request.
    const ByteView bytes = inputs.document(inputs.documents, requested);
    if (!descriptor || !bytes.data || !bytes.size)
        return 0;
    descriptor[0] = 0;
    descriptor[1] = static_cast<unsigned>(bytes.size);
    descriptor[2] = static_cast<unsigned>(reinterpret_cast<uintptr_t>(bytes.data));
    return 1;
}
int __fastcall Available(unsigned id, void* unused, unsigned* file) {
    const int result = availability(id, unused, file);
    return inputs.assets->Availability(id, result, file);
}
int __fastcall Resolve(unsigned id, unsigned* file) {
    const int result = resolveReference(id, file);
    return inputs.assets->ResolveMap(id, result, file, InterlockedCompareExchange(&joined, 0, 0) != 0);
}
unsigned __fastcall Decode(const unsigned short* id, unsigned char* mode) {
    const unsigned decoded = decodeReference(id, mode);
    return inputs.assets->DecodeMap(decoded, InterlockedCompareExchange(&joined, 0, 0) != 0);
}
void __stdcall Joined(const short* result) {
    InterlockedExchange(&joined, 1);
    joinResult(result);
}
} // namespace
NativeResourceInputs MakeNativeResourceInputs(Assets& assets, Content& content, void** main, const std::wstring& path) {
    NativeResourceInputs selected;
    selected.build = 14075;
    selected.membership = {&assets, [](void* value, uint32_t id) { return static_cast<Assets*>(value)->Contains(id); }};
    selected.assets = &assets;
    selected.documents = &content;
    selected.document = [](void* value, const wchar_t* name) { return static_cast<Content*>(value)->Document(name); };
    selected.mainArchive = main;
    selected.localPath = path;
    return selected;
}
bool ConfigureNativeResources(const NativeResourceInputs& selected, std::string& error) {
    error = "April native resources require own providers, private cache and archive slot";
#ifdef _WIN64
    (void)selected;
    return false;
#else
    if (selected.build != 14075 || !selected.assets || !selected.membership.contains || !selected.documents ||
        !selected.document || !selected.mainArchive || selected.localPath.empty() ||
        selected.localPath.size() >= MAX_PATH)
        return false;
    inputs = selected;
    localArchive = nullptr;
    InterlockedExchange(&archiveOpened, 0);
    InterlockedExchange(&fileTableReady, 0);
    InterlockedExchange(&joined, 0);
    InterlockedExchange(&deferred, 0);
    InterlockedExchange(&prepareCalls, 0);
    error.clear();
    return true;
#endif
}
bool NativeArchiveReady() {
    return InterlockedCompareExchange(&prepareCalls, 0, 0) != 0 &&
           InterlockedCompareExchange(&archiveOpened, 0, 0) != 0 &&
           InterlockedCompareExchange(&fileTableReady, 0, 0) != 0 && InterlockedCompareExchange(&deferred, 0, 0) == 0;
}
std::string NativeArchiveDiagnostic() {
    char text[192];
    std::snprintf(text, sizeof(text), "April archive: calls=%ld opened=%ld table=%ld deferred=%ld main=%u",
                  InterlockedCompareExchange(&prepareCalls, 0, 0), InterlockedCompareExchange(&archiveOpened, 0, 0),
                  InterlockedCompareExchange(&fileTableReady, 0, 0), InterlockedCompareExchange(&deferred, 0, 0),
                  inputs.mainArchive && *inputs.mainArchive ? 1u : 0u);
    return text;
}
const std::vector<HookSpec>& NativeResourceHooks() {
    static const std::vector<HookSpec> hooks = [] {
        void* callbacks[] = {
            reinterpret_cast<void*>(Local),   reinterpret_cast<void*>(Write),  reinterpret_cast<void*>(Cipher),
            reinterpret_cast<void*>(Derive),  reinterpret_cast<void*>(Packs),  reinterpret_cast<void*>(Manifest),
            reinterpret_cast<void*>(Lookup),  reinterpret_cast<void*>(Joined), reinterpret_cast<void*>(Available),
            reinterpret_cast<void*>(Resolve), reinterpret_cast<void*>(Decode), reinterpret_cast<void*>(Initialize),
            reinterpret_cast<void*>(Prepare)};
        void** originals[] = {reinterpret_cast<void**>(&openLocal),
                              reinterpret_cast<void**>(&writeRaw),
                              &oldCipher,
                              reinterpret_cast<void**>(&deriveKey),
                              reinterpret_cast<void**>(&openPacks),
                              reinterpret_cast<void**>(&readManifest),
                              &oldLookup,
                              reinterpret_cast<void**>(&joinResult),
                              reinterpret_cast<void**>(&availability),
                              reinterpret_cast<void**>(&resolveReference),
                              reinterpret_cast<void**>(&decodeReference),
                              reinterpret_cast<void**>(&initializeManifest),
                              reinterpret_cast<void**>(&prepareArchive)};
        std::vector<HookSpec> result;
        const auto& symbols = NativeResourceSymbols();
        if (symbols.size() != 14)
            return result;
        for (size_t i = 0; i < 13; ++i)
            result.push_back({symbols[i].name, symbols[i].symbol, callbacks[i], originals[i], true});
        return result;
    }();
    return hooks;
}
} // namespace april14075
} // namespace glide
