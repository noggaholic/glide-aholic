#pragma once
#include "glide/character.h"

namespace glide {
namespace combat {
constexpr unsigned kPinnacleOfStrength = 1453;
inline bool CanSelectCombatTrait(unsigned build, const Character& character) {
    const auto p = character.profileFields.find("profession");
    return (build == 167409 || build == 204489) && p != character.profileFields.end() && p->second == "2";
}
inline bool ReadCombatTrait(unsigned build, const Character& character, unsigned& selected) {
    selected = 0;
    const auto at = character.profileFields.find("combat_trait");
    if (at == character.profileFields.end() || at->second.empty())
        return true;
    if (!CanSelectCombatTrait(build, character) || at->second != "1453")
        return false;
    selected = kPinnacleOfStrength;
    return true;
}
} // namespace combat
} // namespace glide
