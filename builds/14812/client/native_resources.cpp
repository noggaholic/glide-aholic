#include "native_resources.h"
#include "resource_index.h"
#include <Windows.h>
#include <cstring>
#include <cwchar>
namespace glide {
namespace july14812 {
namespace {
uint64_t Now() {
    return GetTickCount64();
}
void Pause() {
    Sleep(1);
}
struct PackOwner {
    void* file = nullptr;
    ~PackOwner() {
        Reset();
    }
    void Reset() {
        if (file) {
            auto table = *static_cast<void***>(file);
            reinterpret_cast<ResourceApi::Stage>(table[0])(file, nullptr);
            file = nullptr;
        }
    }
    bool Create(ResourceApi::Pack create, ByteView bytes) {
        Reset();
        file = create(0, static_cast<unsigned>(bytes.size), bytes.data);
        if (!file)
            return false;
        auto table = *static_cast<void***>(file);
        if (!table || !table[0])
            ExitProcess(3);
        return table[9] != nullptr;
    }
    uint32_t* Chunk(unsigned tag, unsigned version) {
        auto table = *static_cast<void***>(file);
        return static_cast<uint32_t*>(
            reinterpret_cast<ResourceApi::Chunk>(table[9])(file, nullptr, tag, version, nullptr));
    }
};
bool Loaded(const Resources& resources) {
    if (!resources.Root().size)
        return false;
    for (size_t i = 0; i < Resources::kManifestCount; ++i)
        if (!resources.Manifest(i).size || !resources.RecordCount(i) ||
            resources.PrivateId(i) != JULY_RESOURCE_PRIVATE_ID + i)
            return false;
    return true;
}
} // namespace
const std::vector<ResourceSymbol>& ResourceSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<ResourceSymbol> symbols = {
        SYMBOL(packFromMemory),    SYMBOL(registerMfst),      SYMBOL(registerRoot), SYMBOL(archiveWrite),
        SYMBOL(operationComplete), SYMBOL(operationFailed),   SYMBOL(queryCache),   SYMBOL(publishRoot),
        SYMBOL(publishFileSize),   SYMBOL(archiveFindHighIds)};
#undef SYMBOL
    return symbols;
}
bool BindResourceApi(const PeImage& image, void* module, ResourceApi& out, std::string& error) {
    error = "July resource binding requires its original x86 module";
    if (!module || sizeof(void*) != 4 || image.Machine() != Bitness::X86)
        return false;
    uint32_t offsets[10];
    size_t index = 0;
    for (const auto& symbol : ResourceSymbols()) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("July resource RVA is not verified: ") + symbol.name;
            return false;
        }
        offsets[index++] = match.rva;
    }
    auto base = static_cast<uint8_t*>(module);
    ResourceApi candidate;
    candidate.pack = reinterpret_cast<ResourceApi::Pack>(base + offsets[0]);
    candidate.registerManifest = reinterpret_cast<ResourceApi::Stage>(base + offsets[1]);
    candidate.registerRoot = reinterpret_cast<ResourceApi::Stage>(base + offsets[2]);
    candidate.write = reinterpret_cast<ResourceApi::Write>(base + offsets[3]);
    candidate.complete = reinterpret_cast<ResourceApi::Operation>(base + offsets[4]);
    candidate.failed = reinterpret_cast<ResourceApi::Operation>(base + offsets[5]);
    candidate.query = reinterpret_cast<ResourceApi::Query>(base + offsets[6]);
    candidate.publishRoot = reinterpret_cast<ResourceApi::Stage>(base + offsets[7]);
    candidate.publishSize = reinterpret_cast<ResourceApi::Size>(base + offsets[8]);
    candidate.findIds = reinterpret_cast<ResourceApi::FindIds>(base + offsets[9]);
    out = candidate;
    error.clear();
    return true;
}
NativeResources::NativeResources(const ResourceApi& api, ResourceClock clock) : api_(api), clock_(clock) {
    if (!clock_.now)
        clock_.now = Now;
    if (!clock_.pause)
        clock_.pause = Pause;
}
bool NativeResources::Ready() const {
    return sizeof(void*) == 4 && api_.pack && api_.registerManifest && api_.registerRoot && api_.write &&
           api_.complete && api_.failed && api_.findIds && api_.publishSize && api_.query && api_.publishRoot;
}
void NativeResources::Wait(void* operation, uint64_t deadline) const {
    // Retain the source's fatal failure policy. An asynchronous native operation
    // may still own its inputs/outputs; returning would reclaim live buffers.
    if (!operation)
        ExitProcess(3);
    auto table = *static_cast<void***>(operation);
    if (!table || !table[0] || table[4] != reinterpret_cast<void*>(api_.complete) ||
        table[5] != reinterpret_cast<void*>(api_.failed))
        ExitProcess(3);
    while (!api_.complete(operation, nullptr)) {
        if (api_.failed(operation, nullptr) || clock_.now() >= deadline)
            ExitProcess(3);
        clock_.pause();
    }
    reinterpret_cast<ResourceApi::Stage>(table[0])(operation, nullptr);
}
bool NativeResources::Seed(const Resources& resources, void* archive, std::string& error) {
    error = "July private resource cache cannot be seeded";
    if (seeded_ || !Ready() || !Loaded(resources) || !archive)
        return false;
    auto table = *static_cast<void***>(archive);
    if (!table || table[12] != reinterpret_cast<void*>(api_.write))
        return false;
    const uint64_t deadline = clock_.now() + 10000;
    for (size_t i = 0; i < Resources::kManifestCount; ++i) {
        Buffer bytes;
        if (!bytes.Append(resources.Manifest(i))) {
            error = "Cannot retain July private manifest bytes";
            return false;
        }
        void* operation = nullptr;
        const unsigned id = resources.PrivateId(i);
        api_.write(archive, nullptr, id, id, 0, static_cast<unsigned>(bytes.Size()), bytes.Data(), &operation, nullptr,
                   2);
        Wait(operation, deadline);
    }
    seeded_ = true;
    error.clear();
    return true;
}
bool NativeResources::Revisions(const Resources& resources, void* archive, std::string& error) const {
    const uint64_t deadline = clock_.now() + 10000;
    error = "July installed revisions do not match resource metadata";
    if (!archive)
        return false;
    auto table = *static_cast<void***>(archive);
    if (!table || table[10] != reinterpret_cast<void*>(api_.findIds))
        return false;
    const ByteView bytes = resources.Manifest(0);
    JulyResourceIndex manifest;
    if (!july_resource_manifest(bytes.data, bytes.size, &manifest))
        return false;
    Buffer ids, high;
    const size_t size = static_cast<size_t>(manifest.count) * 4;
    if (!ids.Resize(size) || !high.Resize(size))
        return false;
    auto input = reinterpret_cast<unsigned*>(ids.Data());
    auto output = reinterpret_cast<unsigned*>(high.Data());
    for (uint32_t i = 0; i < manifest.count; ++i)
        input[i] = july_resource_word(manifest.rows + static_cast<size_t>(i) * 12);
    void* operation = nullptr;
    api_.findIds(archive, nullptr, input, output, manifest.count, &operation, nullptr, 2);
    Wait(operation, deadline);
    for (uint32_t i = 0; i < manifest.count; ++i)
        if (!output[i] || output[i] != july_resource_word(manifest.rows + static_cast<size_t>(i) * 12 + 4))
            return false;
    return true;
}
bool NativeResources::ProbeAndPublish(const Resources& resources, void* archive, bool publish, std::string& error) {
    error = "July resource publication preconditions failed";
    if (!Ready() || !Loaded(resources) || published_ || (publish && !seeded_))
        return false;
    const auto manifestBytes = resources.Manifest(0), rootBytes = resources.Root();
    JulyResourceIndex manifest, root;
    if (!july_resource_manifest(manifestBytes.data, manifestBytes.size, &manifest) ||
        !july_resource_root(rootBytes.data, rootBytes.size, manifestBytes.size, &root))
        return false;
    error = "Original July PackFile rejected manifest data";
    PackOwner pack;
    if (!pack.Create(api_.pack, manifestBytes))
        return false;
    api_.registerManifest(pack.file, nullptr);
    auto chunk = pack.Chunk(0x5453464d, 2);
    if (!chunk || chunk[0] != 14812 || chunk[1] != manifest.count ||
        chunk[2] != reinterpret_cast<uintptr_t>(chunk + 7) || chunk[3] || chunk[5] ||
        std::memcmp(chunk + 7, manifest.rows, static_cast<size_t>(manifest.count) * 12))
        return false;
    error = "Original July PackFile rejected root data";
    if (!pack.Create(api_.pack, rootBytes))
        return false;
    api_.registerRoot(pack.file, nullptr);
    chunk = pack.Chunk(0x464d5241, 1);
    if (!chunk || chunk[0] != 14812 || chunk[1] != root.count || chunk[2] != reinterpret_cast<uintptr_t>(chunk + 5) ||
        chunk[3])
        return false;
    const auto rows = chunk + 5;
    for (uint32_t i = 0; i < root.count; ++i) {
        const auto encoded = root.rows + static_cast<size_t>(i) * 20;
        const auto expected = encoded + 16 + july_resource_word(encoded + 16);
        const auto relocated = reinterpret_cast<uintptr_t>(chunk) + static_cast<size_t>(expected - rootBytes.data) - 28;
        if (std::memcmp(rows + i * 5, encoded, 16) || rows[i * 5 + 4] != relocated ||
            std::wcscmp(reinterpret_cast<const wchar_t*>(relocated), reinterpret_cast<const wchar_t*>(expected)))
            return false;
    }
    if (publish) {
        if (!Revisions(resources, archive, error))
            return false;
        error = "Original July cache does not confirm installed resource metadata";
        for (size_t i = 0; i < Resources::kManifestCount; ++i) {
            unsigned length = 0, available = 0;
            const unsigned id = resources.PrivateId(i), size = static_cast<unsigned>(resources.Manifest(i).size);
            api_.publishSize(id, id, size);
            if (!api_.query(id, 0, &length, &available) || length != size || available != 1)
                return false;
        }
        // The native root loader must receive a fresh pack without probe views.
        error = "Cannot create fresh July root for publication";
        if (!pack.Create(api_.pack, rootBytes))
            return false;
        api_.publishRoot(pack.file, nullptr);
        published_ = true;
    }
    error.clear();
    return true;
}
} // namespace july14812
} // namespace glide
