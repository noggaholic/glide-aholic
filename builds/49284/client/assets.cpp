#include "assets.h"
#include "../../../client/src/runtime.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <cstring>

namespace glide {
namespace {
struct AssetFile {
    unsigned id;
    const wchar_t* path;
    size_t size;
    uint8_t hash[32];
};
const AssetFile files[] = {
#include "asset_files.inc"
};
HotAssets assets;
using Read = int(__fastcall*)(unsigned*, void*);
using Query = int(__fastcall*)(unsigned, unsigned, unsigned*, unsigned*, unsigned*);
Read originalRead = nullptr;
Query originalQuery = nullptr;
HotAssetChunk chunk = nullptr;
bool prepared = false;
int __fastcall ReadBytes(unsigned* job, void* unused) {
    const ByteView bytes = assets.Find(job[0], job[1]);
    return bytes.size ? ReadHotAsset(bytes, job, chunk) : originalRead(job, unused);
}
int __fastcall QueryCache(unsigned id, unsigned variant, unsigned* revision, unsigned* size, unsigned* cached) {
    const int result = originalQuery(id, variant, revision, size, cached);
    const ByteView bytes = assets.Find(id, variant);
    if (!bytes.size)
        return result;
    if (revision)
        *revision = id;
    if (size)
        *size = static_cast<unsigned>(bytes.size);
    if (cached)
        *cached = 1;
    return 1;
}
} // namespace
bool HotAssets::Load(const std::wstring& directory, std::string& error) {
    std::map<unsigned, Buffer> candidate;
    for (const auto& file : files) {
        Buffer bytes;
        uint8_t hash[32];
        error = "Missing or changed HoT supplemental resource " + std::to_string(file.id);
        if (!ReadFileBytes(directory + L"/" + file.path, file.size, bytes) || bytes.Size() != file.size ||
            !Sha256(bytes.View(), hash) || std::memcmp(hash, file.hash, sizeof(hash)) || candidate.count(file.id))
            return false;
        candidate.emplace(file.id, std::move(bytes));
    }
    files_ = std::move(candidate);
    error.clear();
    return true;
}
ByteView HotAssets::Find(unsigned id, unsigned variant) const {
    if (variant & 255u)
        return ByteView();
    const auto at = files_.find(id);
    return at == files_.end() ? ByteView() : at->second.View();
}
int ReadHotAsset(ByteView asset, unsigned* job, HotAssetChunk consumer) {
    if (!job || !consumer || !asset.Valid() || !asset.size || asset.size > UINT32_MAX)
        return 0;
    const unsigned offset = job[3];
    unsigned count = job[4];
    if (offset > asset.size || count > asset.size - offset || (!job[8] && (!job[2] || !count)))
        return 0;
    if (job[8] && !count)
        count = static_cast<unsigned>(asset.size) - offset;
    consumer(asset.data + offset, count, 0, static_cast<unsigned>(asset.size), 3, job);
    job[6] = 1;
    return job[8] || job[5] == job[4] ? 1 : 0;
}
const std::vector<HotSymbol>& HotAssetSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(hot_overlay_readChunk), SYMBOL(hot_overlay_readBytes),
                                                   SYMBOL(hot_root_cacheQuery)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotAssetHooks() {
    static const std::vector<HookSpec> hooks = {
        {HotAssetSymbols()[1].name, HotAssetSymbols()[1].symbol, reinterpret_cast<void*>(ReadBytes),
         reinterpret_cast<void**>(&originalRead), true},
        {HotAssetSymbols()[2].name, HotAssetSymbols()[2].symbol, reinterpret_cast<void*>(QueryCache),
         reinterpret_cast<void**>(&originalQuery), true}};
    return hooks;
}
bool PrepareHotAssets(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT resources require an unprepared x86 build 49284 client";
    if (prepared || !module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 || runtime.config.build != 49284)
        return false;
    uint32_t chunkRva = 0;
    for (size_t i = 0; i < HotAssetSymbols().size(); ++i) {
        const auto& symbol = HotAssetSymbols()[i];
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("HoT asset RVA is not verified: ") + symbol.name;
            return false;
        }
        if (!i)
            chunkRva = match.rva;
    }
    if (!assets.Load(runtime.config.dataDirectory + L"/content", error))
        return false;
    chunk = reinterpret_cast<HotAssetChunk>(static_cast<uint8_t*>(module) + chunkRva);
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
