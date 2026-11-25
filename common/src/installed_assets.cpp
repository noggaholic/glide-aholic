#include "glide/installed_assets.h"
#include <algorithm>
#include "glide/log.h"
#include "glide/installation_data.h"
namespace glide {
bool InstalledAssets::Open(const std::wstring& path, std::string& error) {
    error = "Invalid installed asset mapping table";
    InstallationData data;
    Buffer bytes;
    if (!data.Open(path, build_, error) || !data.Read("client/texture-pairs.bin", bytes, error))
        return false;
    error = "Invalid local texture mapping table";
    if (!bytes.Size() || bytes.Size() % 8 || bytes.Size() / 8 > 100000)
        return false;
    Reader reader(bytes.View());
    std::vector<InstalledAssetPair> pairs;
    while (reader.Remaining()) {
        InstalledAssetPair pair{};
        if (!reader.U32(pair.reference) || !reader.U32(pair.file) || !pair.reference || !pair.file ||
            (!pairs.empty() && pairs.back().reference >= pair.reference))
            return false;
        pairs.push_back(pair);
    }
    std::unique_ptr<DatArchive> archive(new DatArchive);
    if (!archive->Open(path)) {
        error = "Cannot read the installed archive index";
        return false;
    }
    std::lock_guard<std::mutex> guard(mutex_);
    pairs_ = std::move(pairs);
    archive_ = std::move(archive);
    error.clear();
    return true;
}
bool InstalledAssets::Query(uint32_t id, uint32_t* size) {
    if (size)
        *size = 0;
    std::lock_guard<std::mutex> guard(mutex_);
    uint32_t decoded = 0;
    if (!archive_ || !archive_->DecodedSize(id, decoded))
        return false;
    if (size)
        *size = decoded;
    return true;
}
bool InstalledAssets::Resolve(uint32_t reference, uint32_t* file) {
    if (file)
        *file = 0;
    uint32_t resolved = reference;
    std::lock_guard<std::mutex> guard(mutex_);
    if (!pairs_.empty()) {
        const auto pair =
            std::lower_bound(pairs_.begin(), pairs_.end(), reference,
                             [](const InstalledAssetPair& value, uint32_t id) { return value.reference < id; });
        if (pair != pairs_.end() && pair->reference == reference)
            resolved = pair->file;
    }
    uint32_t size = 0;
    if (!archive_ || !archive_->DecodedSize(reference, size) || !archive_->DecodedSize(resolved, size))
        return false;
    if (file)
        *file = resolved;
    return true;
}
bool InstalledAssets::Variant(uint32_t reference, uint8_t mode, uint32_t* file) {
    if (file)
        *file = 0;
    uint32_t resolved = 0;
    if (mode != 1 || !Resolve(reference, &resolved) || resolved == reference)
        return false;
    if (file)
        *file = resolved;
    return true;
}
void __fastcall InstalledAssets::Destroy(Provider*, void*, unsigned) {}
int __fastcall InstalledAssets::NativeQuery(Provider* self, void*, uint32_t id, uint32_t* size, uint32_t* available) {
    const bool found = self->owner->Query(id, size);
    if (available)
        *available = found ? 1u : 0u;
    return found ? 1 : 0;
}
int __fastcall InstalledAssets::NativeResolve(Provider* self, void*, uint32_t id, uint32_t* file) {
    const bool found = self->owner->Resolve(id, file);
    self->owner->Trace("resolve", id, file ? *file : 0, 0, found);
    return found ? 1 : 0;
}
int __fastcall InstalledAssets::Contains(Provider* self, void*, uint32_t id) {
    return self->owner->Resolve(id) ? 1 : 0;
}
void __fastcall InstalledAssets::Cache(Provider*, void*, uint32_t, unsigned) {}
int __fastcall InstalledAssets::NativeVariant(Provider* self, void*, uint32_t id, uint8_t mode, uint32_t* file) {
    const bool found = self->owner->Variant(id, mode, file);
    self->owner->Trace("variant", id, file ? *file : 0, mode, found);
    return found ? 1 : 0;
}
void InstalledAssets::Trace(const char* operation, uint32_t id, uint32_t file, uint8_t mode, bool found) {
    if (!trace_)
        return;
    const uint64_t key = (uint64_t(static_cast<unsigned char>(*operation)) << 40) | (uint64_t(mode) << 32) | id;
    std::lock_guard<std::mutex> guard(traceMutex_);
    if (traced_.size() >= 8192 || !traced_.insert(key).second)
        return;
    trace_->Write("asset-request", std::string(operation) + " id=" + std::to_string(id) +
                                       " mode=" + std::to_string(mode) + " found=" + (found ? "1" : "0") +
                                       " file=" + std::to_string(file));
}
void* InstalledAssets::NativeManifest() {
#ifdef _WIN64
    return nullptr;
#else
    static void* table[] = {reinterpret_cast<void*>(Destroy),       reinterpret_cast<void*>(NativeQuery),
                            reinterpret_cast<void*>(NativeResolve), reinterpret_cast<void*>(Contains),
                            reinterpret_cast<void*>(Cache),         reinterpret_cast<void*>(NativeVariant)};
    std::lock_guard<std::mutex> guard(mutex_);
    if (!archive_)
        return nullptr;
    provider_.table = table;
    provider_.owner = this;
    return &provider_;
#endif
}
} // namespace glide
