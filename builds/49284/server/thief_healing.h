#pragma once

namespace glide {
namespace hot49284 {
// Own type46/13027, palette268, profession5. Initial source is admitted on land/surface. Water animation alone is
// not interpreted as selection permission. Trait1136 and modern cleansing excluded.
struct ThiefHealing {
    static const unsigned Skill = 13027, Palette = 268, Slot = 0, Activation = 1350, Animation = 2067, Recharge = 30000;
    static const unsigned Stealth = 13017, StealthDuration = 3000, Regeneration = 718, RegenerationDuration = 4000;
    static int Amount(unsigned level) {
        return 120 + static_cast<int>(static_cast<float>(level * level) * 0.8f + 0.5f);
    }
    // Local consumption policy at the own activation marker, not an inferred
    // hit. Revealed890's HoT duration remains unimplemented rather than copied.
    static unsigned AttackActivation(unsigned skill) {
        return skill == 13005 ? 1000 : skill == 13125 ? 827 : 0;
    }
};
} // namespace hot49284
} // namespace glide
