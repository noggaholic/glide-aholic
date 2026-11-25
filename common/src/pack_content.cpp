#include "glide/pack_content.h"
#include "glide/files.h"
#include <algorithm>
#include <cstring>
#include <utility>

namespace glide {
namespace {
bool Range(ByteView bytes, size_t at, size_t length) {
    return bytes.Valid() && at <= bytes.size && length <= bytes.size - at;
}
uint16_t Read16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0] | (unsigned(p[1]) << 8));
}
uint32_t Read32(const uint8_t* p) {
    return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint64_t Read64(const uint8_t* p) {
    return Read32(p) | (uint64_t(Read32(p + 4)) << 32);
}
} // namespace
bool PackRelative(ByteView bytes, size_t at, unsigned pointerSize, size_t& target) {
    if ((pointerSize != 4 && pointerSize != 8) || !Range(bytes, at, pointerSize))
        return false;
    const uint64_t bits = pointerSize == 4 ? Read32(bytes.data + at) : Read64(bytes.data + at);
    if (!bits)
        return false;
    const uint64_t sign = uint64_t(1) << (pointerSize * 8 - 1);
    if (bits & sign) {
        const uint64_t magnitude = pointerSize == 4 ? uint32_t(0 - uint32_t(bits)) : uint64_t(0 - bits);
        if (magnitude > at)
            return false;
        target = at - static_cast<size_t>(magnitude);
    } else {
        if (bits > bytes.size - at)
            return false;
        target = at + static_cast<size_t>(bits);
    }
    return true;
}
bool ReadPackArray(ByteView bytes, size_t at, unsigned pointerSize, size_t stride, PackArray& out) {
    if ((pointerSize != 4 && pointerSize != 8) || !stride || !Range(bytes, at, 4 + pointerSize))
        return false;
    PackArray candidate;
    candidate.count = Read32(bytes.data + at);
    candidate.stride = stride;
    const size_t pointerAt = at + 4;
    const uint64_t relative = pointerSize == 4 ? Read32(bytes.data + pointerAt) : Read64(bytes.data + pointerAt);
    if (relative) {
        if (!PackRelative(bytes, pointerAt, pointerSize, candidate.start))
            return false;
    } else if (candidate.count)
        return false;
    if (candidate.count > (bytes.size - candidate.start) / stride)
        return false;
    out = candidate;
    return true;
}
ByteView PackContent::Content() const {
    const auto& content = arrays_[10];
    return bytes_.Size() ? ByteView(bytes_.Data() + content.start, content.count) : ByteView();
}
bool PackContent::U32(size_t source, uint32_t& out) const {
    const auto content = Content();
    if (!Range(content, source, 4))
        return false;
    out = Read32(content.data + source);
    return true;
}
bool PackContent::F32(size_t source, float& out) const {
    uint32_t value = 0;
    if (!U32(source, value))
        return false;
    std::memcpy(&out, &value, 4);
    return true;
}
bool PackContent::Parse(ByteView bytes, unsigned shard, unsigned shardCount, std::string* error) {
    std::string ignored;
    std::string& reason = error ? *error : ignored;
    reason = "invalid content header";
    if (!Range(bytes, 0, 28) || !shardCount || shard >= shardCount || std::memcmp(bytes.data, "PF", 2) ||
        std::memcmp(bytes.data + 8, "cntcMain", 8) || Read16(bytes.data + 20) != 0)
        return false;
    PackContent candidate;
    candidate.pointerSize_ = Read16(bytes.data + 2) == 5 ? 8 : 4;
    const size_t root = 12u + Read16(bytes.data + 22);
    if (!Range(bytes, root, 4) || (Read32(bytes.data + root) & 1) || !candidate.bytes_.Append(bytes))
        return false;
    if (!candidate.Index(shard, shardCount, reason))
        return false;
    *this = std::move(candidate);
    reason.clear();
    return true;
}
bool PackContent::Sources(size_t arrayIndex, std::vector<uint32_t>& out) {
    const auto& array = arrays_[arrayIndex];
    out.reserve(array.count);
    for (size_t i = 0; i < array.count; ++i) {
        const uint32_t source = Read32(bytes_.Data() + array.start + i * 4);
        if (!Range(Content(), source, 4))
            return false;
        out.push_back(source);
    }
    std::sort(out.begin(), out.end());
    return std::adjacent_find(out.begin(), out.end()) == out.end();
}
bool PackContent::Index(unsigned shard, unsigned shardCount, std::string& error) {
    static const size_t strides[] = {17, 16, 8, 16, 4, 8, 4, 4, 12, 8, 1};
    size_t at = 16u + Read16(bytes_.Data() + 22);
    for (size_t i = 0; i < arrays_.size(); ++i) {
        error = "invalid array " + std::to_string(i);
        const size_t stride = strides[i] - ((pointerSize_ == 4 && (i == 1 || i == 2 || i == 9)) ? 4 : 0);
        if (!ReadPackArray(bytes_.View(), at, pointerSize_, stride, arrays_[i]))
            return false;
        at += 4 + pointerSize_;
    }
    error = "invalid string or filename relocation";
    if (!Sources(7, strings_) || !Sources(6, files_))
        return false;
    error = "invalid content node";
    const auto& entries = arrays_[3];
    nodes_.reserve(entries.count);
    for (size_t i = 0; i < entries.count; ++i) {
        const auto p = bytes_.Data() + entries.start + i * 16;
        ContentNode node{Read32(p), Read32(p + 4)};
        if (node.offset >= Content().size)
            return false;
        nodes_.push_back(node);
    }
    std::sort(nodes_.begin(), nodes_.end(),
              [](const ContentNode& a, const ContentNode& b) { return a.offset < b.offset; });
    error = "duplicate content node offset";
    for (size_t i = 1; i < nodes_.size(); ++i)
        if (nodes_[i - 1].offset == nodes_[i].offset)
            return false;
    for (size_t kind : {size_t(4), size_t(5), size_t(8)}) {
        error = "invalid link array " + std::to_string(kind);
        const auto& array = arrays_[kind];
        for (size_t i = 0; i < array.count; ++i) {
            const auto p = bytes_.Data() + array.start + i * array.stride;
            ContentLink link;
            link.source = Read32(p);
            link.shard = kind == 4 ? shard : Read32(p + 4);
            if (!Range(Content(), link.source, pointerSize_) || link.shard >= shardCount)
                return false;
            const uint64_t target = kind == 8 ? Read32(p + 8)
                                              : (pointerSize_ == 8 ? Read64(Content().data + link.source)
                                                                   : Read32(Content().data + link.source));
            if (target > UINT32_MAX)
                return false;
            link.offset = static_cast<uint32_t>(target);
            // Tracked references register dependencies; they do not relocate this slot.
            if (kind == 8)
                tracked_.push_back(link);
            else
                links_.push_back(link);
        }
    }
    std::sort(links_.begin(), links_.end(),
              [](const ContentLink& a, const ContentLink& b) { return a.source < b.source; });
    error = "conflicting link targets";
    for (size_t i = 1; i < links_.size(); ++i) {
        const auto& a = links_[i - 1];
        const auto& b = links_[i];
        if (a.source == b.source && (a.offset != b.offset || a.shard != b.shard))
            return false;
    }
    return true;
}
const ContentNode* PackContent::Node(uint32_t offset) const {
    const auto found = std::lower_bound(nodes_.begin(), nodes_.end(), offset,
                                        [](const ContentNode& node, uint32_t value) { return node.offset < value; });
    return found != nodes_.end() && found->offset == offset ? &*found : nullptr;
}
const ContentLink* PackContent::Link(uint32_t source) const {
    const auto found = std::lower_bound(links_.begin(), links_.end(), source,
                                        [](const ContentLink& link, uint32_t value) { return link.source < value; });
    return found != links_.end() && found->source == source ? &*found : nullptr;
}
bool PackContent::TypeField(uint32_t type, unsigned field, uint32_t& out) const {
    if (type >= arrays_[0].count || field > 3)
        return false;
    out = Read32(bytes_.Data() + arrays_[0].start + size_t(type) * 17 + field * 4);
    return out != UINT32_MAX;
}
uint32_t PackContent::DataId(const ContentNode& node, const PackContent& main) const {
    uint32_t offset = 0, value = 0;
    const auto& owner = arrays_[0].count ? *this : main;
    return owner.TypeField(node.type, 2, offset) && node.offset <= Content().size &&
                   offset <= Content().size - node.offset && U32(size_t(node.offset) + offset, value)
               ? value
               : 0;
}
bool PackContent::StringIndex(uint32_t index, std::string& out) const {
    if (index >= arrays_[9].count)
        return false;
    size_t cursor = 0;
    if (!PackRelative(bytes_.View(), arrays_[9].start + size_t(index) * pointerSize_, pointerSize_, cursor))
        return false;
    std::wstring wide;
    while (Range(bytes_.View(), cursor, 2) && wide.size() < 4096) {
        const uint16_t ch = Read16(bytes_.Data() + cursor);
        cursor += 2;
        if (!ch) {
            std::string candidate;
            if (wide.empty() || !WideToUtf8(wide, candidate))
                return false;
            out.swap(candidate);
            return true;
        }
        wide.push_back(static_cast<wchar_t>(ch));
    }
    return false;
}
bool PackContent::String(uint32_t source, std::string& out) const {
    uint32_t index = 0;
    return std::binary_search(strings_.begin(), strings_.end(), source) && U32(source, index) &&
           StringIndex(index, out);
}
bool PackContent::FileIndex(uint32_t index, uint32_t& out) const {
    if (index >= arrays_[2].count)
        return false;
    return PackFileReference(bytes_.View(), arrays_[2].start + size_t(index) * pointerSize_, pointerSize_, out);
}
bool PackFileReference(ByteView bytes, size_t at, unsigned pointerSize, uint32_t& out) {
    size_t target = 0;
    if (!PackRelative(bytes, at, pointerSize, target) || !Range(bytes, target, 6))
        return false;
    const uint32_t low = Read16(bytes.data + target), high = Read16(bytes.data + target + 2);
    if (low < 256 || high < 256)
        return false;
    const uint16_t version = Read16(bytes.data + target + 4);
    if (version && (version < 256 || !Range(bytes, target, 8) || Read16(bytes.data + target + 6)))
        return false;
    const uint64_t id = 0xff00ULL * (high - 256) + (low - 256) + 1;
    if (id > UINT32_MAX)
        return false;
    out = static_cast<uint32_t>(id);
    return true;
}
bool PackContent::File(uint32_t source, const PackContent& main, uint32_t& out) const {
    uint32_t index = 0;
    const auto& owner = arrays_[2].count ? *this : main;
    return std::binary_search(files_.begin(), files_.end(), source) && U32(source, index) &&
           owner.FileIndex(index, out);
}
} // namespace glide
