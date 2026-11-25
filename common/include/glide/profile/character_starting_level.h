#pragma once
#include "glide/character.h"

namespace glide {
constexpr unsigned kMaximumCharacterLevel = 80;

// The client's starting recipe is independent of the saved exploration level.
inline bool StartingLoadoutLevel(const Character& saved, unsigned& out) {
    if (!saved.level || saved.level > kMaximumCharacterLevel)
        return false;
    unsigned level = saved.level;
    const auto requested = saved.profileFields.find("requested_level");
    if (requested != saved.profileFields.end() &&
        (!ParseUnsigned(requested->second, kMaximumCharacterLevel, level) || !level))
        return false;
    out = level;
    return true;
}
} // namespace glide
