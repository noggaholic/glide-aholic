#pragma once
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_protection_data.h"
#include "glide/profile/beta_positive_effect_state.h"
#include "glide/profile/beta_rock_barrier_data.h"
#include "glide/character.h"
#include "glide/profile/modern_combat_defense.h"

namespace glide {
namespace beta {
inline unsigned CombatLevel(unsigned build, const Character& saved) {
    unsigned result = saved.level;
    if (BetaHealthBuild(build)) {
        const auto at = saved.profileFields.find("effective_level");
        if (at != saved.profileFields.end()) {
            uint64_t value = 0;
            if (!PositiveDecimal(at->second, value) || value > 80)
                return 0;
            result = static_cast<unsigned>(value);
        }
    }
    return result && result <= 80 ? result : 0;
}
// Fixed reference for the existing provisional damage1 policy, not recovered
// equipment/base armor. Own rock1384 adds10+.5*effectiveLevel per live instance.
constexpr float kCompatibleReferenceArmor = 1000.0f;
// Protection280 model: IncomingDamagePercent base -33 in all four betas.
// Read durable state, never another actor's concurrently running controller.
inline bool CombatDamageAgainst(unsigned build, const std::string& positiveEffects, uint64_t utc, float& damage,
                                unsigned level = 0) {
    damage = 1.0f;
    if (build == 49284 || build == 167409 || build == 204489)
        return modern::DefenseDamage(build, positiveEffects, utc, damage);
    if (!BetaHealthBuild(build) || positiveEffects.compare(0, 3, "b1:"))
        return true;
    StoredPositiveEffects saved;
    if (!utc || !ReadPositiveEffects(positiveEffects, saved))
        return false;
    unsigned rocks = 0, rockEntries = 0;
    for (const auto& effect : saved.effects) {
        if (effect.definition == Protection965::Buff) {
            if (effect.end - effect.start > Protection965::DurationMs)
                return false;
            if (effect.start <= utc && utc < effect.end)
                damage = 0.67f;
        } else if (effect.definition == RockBarrier6224::Buff) {
            if (++rockEntries > RockBarrier6224::Charges || effect.end - effect.start > RockBarrier6224::DurationMs)
                return false;
            if (effect.start <= utc && utc < effect.end)
                ++rocks;
        }
    }
    if (rocks) {
        if (!level || level > 80)
            return false;
        const float bonus = rocks * (10.0f + 0.5f * level);
        damage *= kCompatibleReferenceArmor / (kCompatibleReferenceArmor + bonus);
    }
    return true;
}
} // namespace beta
} // namespace glide
