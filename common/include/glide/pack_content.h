#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
#include <vector>

namespace glide {
struct PackArray {
    uint32_t count = 0;
    size_t start = 0, stride = 0;
};
struct ContentNode {
    uint32_t type = 0, offset = 0;
};
struct ContentLink {
    uint32_t source = 0, shard = 0, offset = 0;
};

bool PackRelative(ByteView bytes, size_t at, unsigned pointerSize, size_t& target);
bool PackFileReference(ByteView bytes, size_t at, unsigned pointerSize, uint32_t& out);
bool ReadPackArray(ByteView bytes, size_t at, unsigned pointerSize, size_t stride, PackArray& out);

// Owns one decrypted PF/cntc shard. All content offsets are relative to Content().
// Parse is atomic; returned node/link pointers remain valid until the next successful Parse.
class PackContent {
  public:
    bool Parse(ByteView bytes, unsigned shard, unsigned shardCount, std::string* error = nullptr);
    unsigned PointerSize() const {
        return pointerSize_;
    }
    ByteView Bytes() const {
        return bytes_.View();
    }
    ByteView Content() const;
    const PackArray* Array(size_t index) const {
        return index < arrays_.size() ? &arrays_[index] : nullptr;
    }
    const std::vector<ContentNode>& Nodes() const {
        return nodes_;
    }
    const std::vector<ContentLink>& Tracked() const {
        return tracked_;
    }
    const ContentNode* Node(uint32_t offset) const;
    const ContentLink* Link(uint32_t source) const;
    bool U32(size_t source, uint32_t& out) const;
    bool F32(size_t source, float& out) const;
    bool TypeField(uint32_t type, unsigned field, uint32_t& out) const;
    uint32_t DataId(const ContentNode& node, const PackContent& main) const;
    bool String(uint32_t source, std::string& out) const;
    bool File(uint32_t source, const PackContent& main, uint32_t& out) const;

  private:
    bool Index(unsigned shard, unsigned shardCount, std::string& error);
    bool Sources(size_t array, std::vector<uint32_t>& out);
    bool StringIndex(uint32_t index, std::string& out) const;
    bool FileIndex(uint32_t index, uint32_t& out) const;
    Buffer bytes_;
    unsigned pointerSize_ = 0;
    std::array<PackArray, 11> arrays_{};
    std::vector<ContentNode> nodes_;
    std::vector<ContentLink> links_, tracked_;
    std::vector<uint32_t> strings_, files_;
};
} // namespace glide
