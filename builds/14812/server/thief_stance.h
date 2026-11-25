#pragma once
#include "glide/profile/beta_positive_effects.h"
#include "../../14505/server/skill_catalog.h"
namespace glide {
namespace july14812 {
// The effect window reuses the bounded HoT adaptation, not historical combat.
struct ThiefStanceRules {
    static const unsigned kCreate = 313, kRemove = 314;
    static unsigned Duration(unsigned buff, unsigned profession) {
        return buff == 8964 && profession == 5 ? 2033 : 0;
    }
    static unsigned SkillDuration(unsigned skill) {
        return skill == 8962 ? 2033 : 0;
    }
    static unsigned SkillBuff(unsigned skill) {
        return skill == 8962 ? 8964 : 0;
    }
    static bool IsStance(unsigned buff) {
        return buff == 8964;
    }
};
using ThiefStance = beta::PositiveEffects<ThiefStanceRules>;
inline const june14505::SkillCandidate& ThiefStanceFollowup() {
    static const june14505::SkillCandidate candidate = [] {
        june14505::SkillCandidate c;
        c.loadout = 6;
        c.weaponSlot = 24;
        c.slot = 7;
        c.skill = 11282;
        c.underwater = true;
        c.hasRange = true;
        c.rangeMin = 0;
        c.rangeMax = 0;
        c.skillFlags = "AllowPlayerControl AnimateUpperBody Aquatic";
        june14505::SkillAnimationPhase phase;
        phase.index = 0;
        phase.type = 0;
        phase.flags = 128;
        phase.duration = 40;
        phase.speed = 1;
        c.phases.push_back(phase);
        return c;
    }();
    return candidate;
}
} // namespace july14812
} // namespace glide
