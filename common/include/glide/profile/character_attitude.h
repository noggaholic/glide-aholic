#pragma once
#include "glide/buffer.h"
#include <utility>

namespace glide {
// This is the subject's attitude toward the recipient's controlled character.
inline bool CharacterAttitude(unsigned build, uint16_t subject, bool hostile, Buffer& out) {
    uint16_t id = 0;
    switch (build) {
    case 14075:
        id = 47;
        break;
    case 14422:
    case 14505:
    case 14812:
        id = 61;
        break;
    case 49284:
        id = 121;
        break;
    case 167409:
        id = 150;
        break;
    case 204489:
        id = 151;
        break;
    default:
        return false;
    }
    if (!subject)
        return false;
    Buffer result;
    if (!result.U16(id) || !result.VarU32(subject) || !result.VarU32(hostile ? 1 : 0))
        return false;
    out = std::move(result);
    return true;
}
} // namespace glide
