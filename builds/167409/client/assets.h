#pragma once
#include "glide/dat_archive.h"
#include <atomic>
#include <map>
#include <mutex>
#include <string>
namespace glide {
namespace china167409 {
// The original archive remains open for read-only metadata and texture-header reads.
class SourceAssets {
  public:
    bool Load(const std::wstring& path, std::string& error);
    bool Contains(uint32_t id) const;
    bool Size(uint32_t id, uint32_t& size);
    uint32_t TextureVariant(uint32_t id);
    void ContentReady() {
        ready_ = true;
    }
    bool Ready() const {
        return ready_;
    }
    size_t MappingCount() const {
        return archive_.MappingCount();
    }

  private:
    bool Entry(uint32_t id, ArchiveEntry& entry) const;
    DatArchive archive_;
    std::mutex mutex_;
    std::map<uint32_t, uint32_t> variants_;
    std::atomic<bool> ready_{false};
};
uint32_t AssetReferenceId(ByteView reference);
bool TextureVariantHeaders(ByteView reference, ByteView full);
} // namespace china167409
} // namespace glide
