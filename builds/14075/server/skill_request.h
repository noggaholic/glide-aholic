#pragma once
#include "motion.h"

namespace glide {
namespace april14075 {
enum class SkillRequestKind : uint8_t { Directed = 0, Ground = 1 };
struct SkillRequest {
    SkillRequestKind kind = SkillRequestKind::Directed;
    uint16_t agent = 0;
    uint32_t sequence = 0, rawIdentity = 0, target = 0;
    uint8_t state = 0, slot = 0;
    std::array<float, 3> point{}; // Original client units, without catalog conversion.
    size_t consumed = 0;
    bool Predicted() const {
        return (rawIdentity & 0x80000000u) != 0;
    }
    uint32_t Identity() const {
        return rawIdentity & 0x7fffffffu;
    }
};
// Recognize original directed/ground shapes; leave other control signals alone.
// Catalog slot, identity, ownership and skill selection policy belongs to the caller.
DecodeStatus DecodeSkillRequest(ByteView input, SkillRequest& out);
} // namespace april14075
} // namespace glide
