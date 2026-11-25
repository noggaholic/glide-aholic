#pragma once
#include "glide/profile/modern_health.h"
#include "glide/profile/regeneration.h"
#include "glide/profile/modern_combat_defense.h"
#include <cmath>
namespace glide {
namespace combat {
struct InfuseLight {
    enum : unsigned { Skill = 27228, Buff = 27737 };
    // HoT4000/Current3000. China3000 is an explicit compatible fallback.
    static unsigned Duration(unsigned build) {
        return build == 49284 ? 4000 : build == 167409 || build == 204489 ? 3000 : 0;
    }
    static bool Active(unsigned build, const std::string& effects, uint64_t utc, bool& active) {
        active = false;
        if (effects.empty() || !Duration(build))
            return true;
        std::string rows;
        if (!modern::PositiveEffectRows(effects, rows))
            return false;
        unsigned count = 0;
        if (!modern::Regeneration::StoredBuffCount(rows, Buff, utc, Duration(build), count))
            return false;
        active = count != 0;
        return true;
    }
    static bool HitHealth(unsigned build, const std::string& effects, uint64_t utc, float before, float damage,
                          float maximum, std::string& after) {
        bool convert = false;
        if (!std::isfinite(before) || before <= 0 || before > maximum || !std::isfinite(damage) || damage <= 0 ||
            !Active(build, effects, utc, convert))
            return false;
        return modern::WriteHealthState((std::min)(maximum, (std::max)(0.0f, before + (convert ? damage : -damage))),
                                        after, maximum);
    }
};
} // namespace combat
} // namespace glide
