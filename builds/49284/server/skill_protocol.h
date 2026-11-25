#pragma once
#include "../../14812/server/motion.h"
#include <cmath>

namespace glide {
namespace hot49284 {

inline bool SkillEnd(uint16_t actor, uint32_t sequence, uint32_t identity, Buffer& out) {
    if (!actor || !sequence || sequence == UINT32_MAX || !identity || (identity & 0x80000000u))
        return false;
    Buffer result;
    if (!result.U16(66) || !result.U16(actor) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(1) || !result.U8(48))
        return false;
    return out.Append(result.View());
}
inline bool SkillPhase(uint16_t actor, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                       uint32_t target, const july14812::SkillStage& stage, bool initial, bool final,
                       const july14812::SkillMovement* move, bool underwater, Buffer& out) {
    if (!actor || !sequence || sequence == UINT32_MAX || !identity || (identity & 0x80000000u) || !skill ||
        slot >= 17 || !stage.durationMs || stage.index >= 32 || stage.type > 1)
        return false;
    Buffer result;
    if (!result.U16(move ? 43 : 24) || !result.U16(actor) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(initial ? 13 : 4) || !result.U8(93) || !result.U8(initial ? 0 : 4))
        return false;
    if (initial) {
        if (!result.U32(skill) || !result.U8(slot) || !result.U8(stage.index) || !result.U32(target) ||
            !result.U8(underwater ? 3 : 1))
            return false;
    } else if (!result.U8(stage.index) || !result.U8(3))
        return false;
    // PeerSteps already supplies the actor's current position before this order.
    // Retain that pose; HoT's optional pose has a separate, larger wire format.
    if (!result.U8(0))
        return false;
    uint16_t pitch = 0x7fff, yaw = 0x7fff;
    if (move) {
        if (!std::isfinite(move->speed) || move->speed < 1 || double(move->speed) >= 4294967296.0)
            return false;
        double directionLength = 0, facingLength = 0;
        for (float value : move->direction) {
            if (!std::isfinite(value) || !result.F32(value))
                return false;
            directionLength += double(value) * value;
        }
        for (float value : move->facing) {
            if (!std::isfinite(value))
                return false;
            facingLength += double(value) * value;
        }
        if (directionLength < 0.000001 || facingLength < 0.000001)
            return false;
        const double scale = 32766 / 3.14159265358979323846;
        pitch = static_cast<uint16_t>(static_cast<int>(
            std::round(std::atan2(move->facing[2], std::hypot(move->facing[0], move->facing[1])) * scale)));
        yaw = static_cast<uint16_t>(static_cast<int>(std::round(std::atan2(move->facing[1], move->facing[0]) * scale)));
    }
    if (!result.U16(pitch) || !result.U16(yaw))
        return false;
    if (move) {
        if (!result.U8(1) || !result.VarU32(static_cast<uint32_t>(move->speed)) || !result.VarU32(stage.durationMs) ||
            !result.U8(0))
            return false;
    } else if (!result.U16(stage.durationMs))
        return false;
    if (final && !SkillEnd(actor, sequence, identity, result))
        return false;
    return out.Append(result.View());
}
inline bool SkillRecharge(uint32_t actor, uint32_t skill, uint32_t remaining, uint32_t total, Buffer& out) {
    if (!actor || !skill || remaining > total)
        return false;
    Buffer result;
    if (!result.U16(435) || !result.VarU32(actor) || !result.VarU32(skill) || !result.VarU32(remaining) ||
        !result.VarU32(total))
        return false;
    return out.Append(result.View());
}
} // namespace hot49284
} // namespace glide
