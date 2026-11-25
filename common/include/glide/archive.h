#pragma once
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace glide {
// Read-only physical archive access. DAT indexes and decompression are separate.
// A failed read may have written a prefix to the caller's buffer.
class ArchiveReader {
  public:
    ArchiveReader() = default;
    ~ArchiveReader() {
        Close();
    }
    ArchiveReader(const ArchiveReader&) = delete;
    ArchiveReader& operator=(const ArchiveReader&) = delete;
    bool Open(const std::wstring& path, bool allowWriter = false);
    void Close();
    bool Read(uint64_t offset, void* data, size_t size);
    uint64_t Size() const {
        return size_;
    }
    bool Chunked() const {
        return !chunks_.empty();
    }

  private:
    std::wstring ChunkPath(uint64_t index) const;
    FILE* file_ = nullptr;
    std::wstring path_;
    std::vector<uint32_t> chunks_;
    uint64_t size_ = 0;
};
} // namespace glide
