#include "skill_request.h"
#include <cmath>
#include <cstring>

namespace glide {
namespace april14075 {
DecodeStatus DecodeSkillRequest(ByteView input, SkillRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader header(input);
    uint16_t id = 0;
    if (!header.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 16)
        return DecodeStatus::Unknown;
    MotionMessage motion;
    const auto status = april14075::DecodeMotion(input, motion);
    if (status != DecodeStatus::Complete)
        return status;
    if (!motion.input.size)
        return DecodeStatus::Unknown;
    const uint8_t tag = motion.input.data[0];
    const bool directed = tag == 0x36 && motion.input.size == 6;
    const bool ground = tag == 0x37 && motion.input.size == 14;
    if (!directed && !ground)
        return DecodeStatus::Unknown;
    SkillRequest next;
    next.kind = ground ? SkillRequestKind::Ground : SkillRequestKind::Directed;
    next.agent = motion.agent;
    next.sequence = motion.sequence;
    next.state = motion.state;
    next.rawIdentity = motion.flags;
    next.consumed = motion.consumed;
    Reader payload(motion.input);
    uint8_t ignored = 0;
    if (!payload.U8(ignored))
        return DecodeStatus::Malformed;
    if (directed) {
        if (!payload.U32(next.target))
            return DecodeStatus::Malformed;
    } else
        for (float& value : next.point) {
            uint32_t bits = 0;
            if (!payload.U32(bits))
                return DecodeStatus::Malformed;
            std::memcpy(&value, &bits, sizeof(value));
            if (!std::isfinite(value))
                return DecodeStatus::Malformed;
        }
    if (!payload.U8(next.slot) || payload.Position() != motion.input.size)
        return DecodeStatus::Malformed;
    out = next;
    return DecodeStatus::Complete;
}
} // namespace april14075
} // namespace glide
