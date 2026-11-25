#include "glide/dat_archive.h"
#include "glide/inflate.h"
#include <algorithm>
#include <cstring>

namespace glide {
namespace {
constexpr size_t Maximum = 512u * 1024u * 1024u;
uint32_t U32(const uint8_t* p) {
    return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint64_t U64(const uint8_t* p) {
    return U32(p) | (uint64_t(U32(p + 4)) << 32);
}
} // namespace
void DatArchive::Close() {
    archive_.Close();
    Buffer empty;
    mft_.Swap(empty);
    mappings_.clear();
    entryCount_ = 0;
}
bool DatArchive::Open(const std::wstring& path) {
    Close();
    if (archive_.Open(path) && Load())
        return true;
    Close();
    return false;
}
bool DatArchive::Load() {
    uint8_t header[40]{};
    if (!archive_.Read(0, header, sizeof(header)) || std::memcmp(header + 1, "AN\x1a", 3))
        return false;
    const size_t length = U32(header + 32);
    const uint64_t offset = U64(header + 24);
    if (length < 72 || length > Maximum || offset > archive_.Size() || length > archive_.Size() - offset ||
        !mft_.Resize(length) || !archive_.Read(offset, mft_.Data(), length) || std::memcmp(mft_.Data(), "Mft\x1a", 4))
        return false;
    const uint32_t declared = U32(mft_.Data() + 12);
    const uint32_t available = static_cast<uint32_t>((length - 24) / 24);
    // Old archives can retain a stale count. Use the bounded physical table as in the beta reader.
    entryCount_ = declared > 1 && declared - 1 <= available ? declared - 1 : available;
    Buffer mapping;
    if (!ReadEntry(2, mapping) || mapping.Size() % 8 || !mapping.Size())
        return false;
    mappings_.reserve(mapping.Size() / 8);
    for (size_t at = 0; at < mapping.Size(); at += 8) {
        const uint32_t id = U32(mapping.Data() + at), index = U32(mapping.Data() + at + 4);
        if (!id && !index)
            continue;
        if (!id || !index || index > entryCount_)
            return false;
        mappings_.push_back({id, index});
    }
    std::sort(mappings_.begin(), mappings_.end(),
              [](const Mapping& a, const Mapping& b) { return a.fileId < b.fileId; });
    for (size_t i = 1; i < mappings_.size(); ++i)
        if (mappings_[i - 1].fileId == mappings_[i].fileId && mappings_[i - 1].index != mappings_[i].index)
            return false;
    return !mappings_.empty();
}
bool DatArchive::Entry(uint32_t index, ArchiveEntry& out) const {
    if (!index || index > entryCount_)
        return false;
    const uint8_t* p = mft_.Data() + size_t(index) * 24;
    ArchiveEntry candidate;
    candidate.offset = U64(p);
    candidate.size = U32(p + 8);
    candidate.compression = static_cast<uint16_t>(p[12] | (unsigned(p[13]) << 8));
    if (!candidate.offset || !candidate.size || candidate.offset > archive_.Size() ||
        candidate.size > archive_.Size() - candidate.offset)
        return false;
    out = candidate;
    return true;
}
uint32_t DatArchive::Find(uint32_t fileId) const {
    const auto found = std::lower_bound(mappings_.begin(), mappings_.end(), fileId,
                                        [](const Mapping& a, uint32_t id) { return a.fileId < id; });
    return found != mappings_.end() && found->fileId == fileId ? found->index : 0;
}
bool DatArchive::Read(uint32_t fileId, Buffer& out, size_t prefix) {
    return ReadEntry(Find(fileId), out, prefix);
}
bool DatArchive::DecodedSize(uint32_t fileId, uint32_t& out) {
    ArchiveEntry entry;
    if (!Entry(Find(fileId), entry) || (entry.compression != 0 && entry.compression != 8))
        return false;
    uint32_t size = entry.size;
    if (entry.compression == 8) {
        uint8_t header[8];
        if (entry.size < 12 || !archive_.Read(entry.offset, header, sizeof(header)))
            return false;
        size = U32(header + 4);
        if (!size || size > Maximum)
            return false;
    }
    out = size;
    return true;
}
bool DatArchive::ReadEntry(uint32_t index, Buffer& out, size_t prefix) {
    ArchiveEntry entry;
    if (!Entry(index, entry) || entry.size > Maximum || (entry.compression != 0 && entry.compression != 8))
        return false;
    Buffer stored;
    if (!entry.compression) {
        const size_t wanted = prefix ? (std::min)(prefix, size_t(entry.size)) : entry.size;
        if (!stored.Resize(wanted) || !archive_.Read(entry.offset, stored.Data(), wanted))
            return false;
        stored.Swap(out);
        return true;
    }
    const size_t padded = (size_t(entry.size) + 3) & ~size_t(3);
    return stored.Resize(padded) && archive_.Read(entry.offset, stored.Data(), entry.size) &&
           InflateArchive(stored.View(), out, Maximum, prefix);
}
} // namespace glide
