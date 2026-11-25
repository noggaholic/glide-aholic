#pragma once
#include "glide/buffer.h"
#include "world_protocol.h"
#include <utility>

namespace glide {
namespace june14505 {
// June Character S276/S267, verified against original handlers and schemas.
// Same fields/units as the existing July codec; character is not a player slot.
inline bool EncodeSkillRechargeForBuild(unsigned build, uint32_t character, uint32_t skill, uint32_t remainingMs,
                                        uint32_t totalMs, Buffer& out) {
    if (!WorldProtocolBuild(build) || !character || !skill || remainingMs > totalMs)
        return false;
    Buffer result;

    if (!result.U16(build == 14075 ? 223 : 276) || !result.VarU32(character) || !result.VarU32(skill) ||
        !result.VarU32(remainingMs) || !result.VarU32(totalMs))
        return false;
    out = std::move(result);
    return true;
}
inline bool EncodeSkillRechargeClearForBuild(unsigned build, uint32_t character, uint32_t skill, Buffer& out) {
    if (!WorldProtocolBuild(build) || !character || !skill)
        return false;
    Buffer result;
    if (!result.U16(build == 14075 ? 214 : 267) || !result.VarU32(character) || !result.VarU32(skill))
        return false;
    out = std::move(result);
    return true;
}
inline bool EncodeSkillRecharge(uint32_t character, uint32_t skill, uint32_t remainingMs, uint32_t totalMs,
                                Buffer& out) {
    return EncodeSkillRechargeForBuild(14505, character, skill, remainingMs, totalMs, out);
}
inline bool EncodeSkillRechargeClear(uint32_t character, uint32_t skill, Buffer& out) {
    return EncodeSkillRechargeClearForBuild(14505, character, skill, out);
}
} // namespace june14505
} // namespace glide
