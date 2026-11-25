#pragma once
#include "glide/profile/beta_combat_defense.h"
#include "glide/profile/beta_siren_data.h"
#include "glide/profile/combat_traits.h"
#include <cmath>

namespace glide {
namespace combat {
// Extend the existing provisional damage1 model, not a recovered weapon formula.
// July347 power modifier:5+.375*recipient level per active contribution.
// Modern740 selects the two own baseline rows with zero trailing context data:
// 5/.3125, interpreted linearly. Raw enum semantics and full attributes remain
// open. Trait1453 enables its own row only with an explicit saved choice;
// other conditional rows remain disabled. See modern-might-model-raw.
// Reference power1000 is an explicit compatibility policy, like reference armor.
constexpr float kCompatibleReferencePower = 1000.0f;
inline bool AttackMultiplier(unsigned build, const std::string& effects, uint64_t utc, unsigned level,
                             float& multiplier, unsigned selectedTrait = 0) {
    multiplier = 1.0f;
    if (selectedTrait && (selectedTrait != kPinnacleOfStrength || (build != 167409 && build != 204489)))
        return false;
    unsigned stacks = 0;
    float perLevel = 0;
    if (build == 14812) {
        if (effects.compare(0, 3, "b1:"))
            return true;
        beta::StoredPositiveEffects saved;
        if (!utc || !beta::ReadPositiveEffects(effects, saved))
            return false;
        for (const auto& effect : saved.effects)
            if (effect.definition == beta::Siren4606::Might) {
                if (effect.end - effect.start > beta::Siren4606::Duration(beta::Siren4606::Might))
                    return false;
                if (effect.start <= utc && utc < effect.end)
                    ++stacks;
            }
        perLevel = 0.375f;
    } else if (build == 49284 || build == 167409 || build == 204489) {
        if (effects.empty() ||
            (effects.compare(0, 2, "1:") && effects.compare(0, 2, "2:") && effects.compare(0, 3, "H1:") &&
             effects.compare(0, 3, "H2:") && effects.compare(0, 3, "H3:")))
            return true;
        std::string rows;
        if (!modern::PositiveEffectRows(effects, rows) ||
            !modern::Regeneration::StoredBuffCount(rows, 740, utc, build == 49284 ? 5000 : 12000, stacks))
            return false;
        perLevel = 0.3125f;
    } else {
        return true;
    }
    if (stacks) {
        if (!level || level > 80)
            return false;
        const float extra = selectedTrait == kPinnacleOfStrength ? 1.0f + 0.1125f * level : 0.0f;
        multiplier += stacks * (5.0f + perLevel * level + extra) / kCompatibleReferencePower;
    }
    return std::isfinite(multiplier) && multiplier >= 1;
}
inline bool HitDamage(unsigned build, const std::string& sourceEffects, unsigned sourceLevel,
                      const std::string& targetEffects, unsigned targetLevel, uint64_t utc, float& damage,
                      unsigned selectedTrait = 0) {
    float attack = 0, defense = 0;
    if (!AttackMultiplier(build, sourceEffects, utc, sourceLevel, attack, selectedTrait) ||
        !beta::CombatDamageAgainst(build, targetEffects, utc, defense, targetLevel))
        return false;
    damage = attack * defense;
    return std::isfinite(damage) && damage > 0;
}
} // namespace combat
} // namespace glide
