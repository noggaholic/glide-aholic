#include "assets.h"
#include "glide/crypto.h"
#include <cstring>
#include <limits>
namespace glide {
namespace china167409 {
namespace {
constexpr uint32_t MaximumAsset = 128u * 1024u * 1024u;
unsigned U16(const uint8_t* bytes) {
    return bytes[0] | (unsigned(bytes[1]) << 8);
}
} // namespace
uint32_t AssetReferenceId(ByteView bytes) {
    // Reuse the native six/eight-byte reference format; do not read beyond a six-byte terminator.
    if (!bytes.data || bytes.size < 6)
        return 0;
    const unsigned low = U16(bytes.data), high = U16(bytes.data + 2), version = U16(bytes.data + 4);
    if (low <= 0xff || high <= 0xff || (version && (version <= 0xff || bytes.size < 8 || U16(bytes.data + 6))))
        return 0;
    return low - 0xff00ffu + high * 0xff00u;
}
bool TextureVariantHeaders(ByteView reference, ByteView full) {
    // Resolve both texture headers explicitly; adjacency alone is insufficient.
    if (!reference.data || !full.data || reference.size < 16 || full.size < 16 ||
        std::memcmp(reference.data, "ATEX", 4) || std::memcmp(full.data, "ATEX", 4) ||
        std::memcmp(reference.data + 4, full.data + 4, 4))
        return false;
    const unsigned width = U16(reference.data + 8), height = U16(reference.data + 10);
    return width >= 4 && height >= 4 && U16(full.data + 8) == width * 2 && U16(full.data + 10) == height * 2;
}
bool SourceAssets::Entry(uint32_t id, ArchiveEntry& entry) const {
    return id && archive_.Entry(archive_.Find(id), entry) && entry.size <= MaximumAsset &&
           (entry.compression == 0 || entry.compression == 8);
}
bool SourceAssets::Contains(uint32_t id) const {
    ArchiveEntry entry;
    return Entry(id, entry);
}
bool SourceAssets::Load(const std::wstring& path, std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    ready_ = false;
    variants_.clear();
    error = "Cannot read the original China archive index or pinned audio preset";
    if (!archive_.Open(path))
        return false;
    Buffer audio;
    uint8_t digest[32];
    const uint8_t expected[] = {0xa5, 0xd6, 0xb4, 0x9a, 0x0f, 0x38, 0x9b, 0x00, 0x9a, 0x97, 0x27,
                                0x40, 0x5a, 0x6d, 0x5e, 0xaa, 0x07, 0x26, 0x82, 0x36, 0x18, 0x87,
                                0x57, 0x9c, 0xea, 0x57, 0x4f, 0x1d, 0xe1, 0xad, 0x63, 0x78};
    if (!archive_.Read(590410, audio) || audio.Size() != 962 || !Sha256(audio.View(), digest) ||
        std::memcmp(expected, digest, sizeof(expected))) {
        archive_.Close();
        return false;
    }
    error.clear();
    return true;
}
bool SourceAssets::Size(uint32_t id, uint32_t& size) {
    std::lock_guard<std::mutex> lock(mutex_);
    ArchiveEntry entry;
    uint32_t selected = 0;
    if (!Entry(id, entry) || !archive_.DecodedSize(id, selected) || !selected || selected > MaximumAsset)
        return false;
    size = selected;
    return true;
}
uint32_t SourceAssets::TextureVariant(uint32_t id) {
    if (id == std::numeric_limits<uint32_t>::max())
        return 0;
    std::lock_guard<std::mutex> lock(mutex_);
    const auto previous = variants_.find(id);
    if (previous != variants_.end())
        return previous->second;
    ArchiveEntry reference, full;
    Buffer left, right;
    uint32_t variant = 0, leftSize = 0, rightSize = 0;
    if (Entry(id, reference) && Entry(id + 1, full) && reference.size >= 16 && full.size >= 16 &&
        reference.size <= 16u * 1024u * 1024u && full.size <= 16u * 1024u * 1024u &&
        archive_.DecodedSize(id, leftSize) && archive_.DecodedSize(id + 1, rightSize) &&
        (!reference.compression || leftSize <= 32u * 1024u * 1024u) &&
        (!full.compression || rightSize <= 32u * 1024u * 1024u) && archive_.Read(id, left, 16) &&
        archive_.Read(id + 1, right, 16) && TextureVariantHeaders(left.View(), right.View()))
        variant = id + 1;
    variants_.emplace(id, variant);
    return variant;
}
} // namespace china167409
} // namespace glide
