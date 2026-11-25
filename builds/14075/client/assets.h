#pragma once
#include "glide/dat_archive.h"
#include <memory>
#include <mutex>

namespace glide {
namespace april14075 {
struct ArchiveResolution {
    uint32_t reference = 0, file = 0;
    uint16_t referenceWidth = 0, referenceHeight = 0, fileWidth = 0, fileHeight = 0;
    std::string slot, resource;
};
struct ArchiveMembership {
    void* context = nullptr;
    bool (*contains)(void*, uint32_t) = nullptr;
    bool Has(uint32_t id) const {
        return id && contains && contains(context, id);
    }
};
class Assets {
  public:
    bool Open(const std::wstring& path, std::string& error);
    bool Contains(uint32_t id);
    int Availability(uint32_t reference, int stockResult, uint32_t* file);
    int ResolveMap(uint32_t reference, int stockResult, uint32_t* file, bool worldJoined);
    uint32_t DecodeMap(uint32_t decoded, bool worldJoined);

  private:
    std::mutex mutex_;
    std::unique_ptr<DatArchive> archive_;
    std::vector<ArchiveResolution> resolutions_;
    const ArchiveResolution* Find(uint32_t reference) const;
};
} // namespace april14075
} // namespace glide
