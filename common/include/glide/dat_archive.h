#pragma once
#include "glide/archive.h"
#include "glide/buffer.h"

namespace glide {
struct ArchiveEntry {
    uint64_t offset = 0;
    uint32_t size = 0;
    uint16_t compression = 0;
};
class DatArchive {
  public:
    bool Open(const std::wstring& path);
    void Close();
    bool Entry(uint32_t index, ArchiveEntry& out) const;
    uint32_t Find(uint32_t fileId) const;
    // Declared uncompressed size, without decompressing the resource. Serialize
    // with other reads when sharing an archive between threads.
    bool DecodedSize(uint32_t fileId, uint32_t& out);
    bool Read(uint32_t fileId, Buffer& out, size_t prefix = 0);
    bool ReadEntry(uint32_t index, Buffer& out, size_t prefix = 0);
    size_t MappingCount() const {
        return mappings_.size();
    }
    bool MappingAt(size_t index, uint32_t& logical, uint32_t& physical) const {
        if (index >= mappings_.size())
            return false;
        logical = mappings_[index].fileId;
        physical = mappings_[index].index;
        return true;
    }
    uint32_t EntryCount() const {
        return entryCount_;
    }

  private:
    struct Mapping {
        uint32_t fileId;
        uint32_t index;
    };
    bool Load();
    ArchiveReader archive_;
    Buffer mft_;
    uint32_t entryCount_ = 0;
    std::vector<Mapping> mappings_;
};
} // namespace glide
