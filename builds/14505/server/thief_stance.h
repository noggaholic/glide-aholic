#pragma once

#include "skill_catalog.h"
#include "glide/profile/beta_positive_effects.h"
#include "../../14075/server/thief_stance_wire.h"
namespace glide {
namespace june14505 {
inline bool ThiefStanceBuild(unsigned build) {
    return build == 14075 || build == 14422 || build == 14505;
}
inline uint16_t ThiefStanceCreate(unsigned build) {
    return static_cast<uint16_t>(build == 14075 ? april14075::kThiefStanceCreate : 313);
}
inline uint16_t ThiefStanceRemove(unsigned build) {
    return static_cast<uint16_t>(build == 14075 ? april14075::kThiefStanceRemove : 314);
}
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
inline const SkillCandidate& ThiefStanceFollowup(unsigned build = 14505) {
    static const SkillCandidate candidate = [] {
        SkillCandidate c;
        c.loadout = 6;
        c.weaponSlot = 24;
        c.slot = 7;
        c.skill = 11282;
        c.underwater = true;
        c.hasRange = true;
        c.rangeMin = 0;
        c.rangeMax = 0;
        c.skillFlags = "AllowPlayerControl AnimateUpperBody Aquatic";
        SkillAnimationPhase phase;
        phase.index = 0;
        phase.type = 0;
        phase.flags = 128;
        phase.duration = 40;
        phase.speed = 1;
        c.phases.push_back(phase);
        return c;
    }();
    static const SkillCandidate april = [] {
        auto c = candidate;
        c.phases[0].flags = april14075::kThiefFollowupPhaseFlags;
        return c;
    }();
    return build == 14075 ? april : candidate;
}
} // namespace june14505
} // namespace glide
