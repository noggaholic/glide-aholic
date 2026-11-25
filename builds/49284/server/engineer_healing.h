#pragma once

namespace glide {
namespace hot49284 {
// Own palette276 and water animation support the explicitly local initial aquatic assignment; the original medium
// eligibility mask has not been recovered here.
struct EngineerHealing {
    static const unsigned Skill = 5834, Palette = 276, Slot = 0, Activation = 1100, Animation = 2333, Recharge = 25000;
    static int Amount(unsigned level) {
        return 120 + static_cast<int>(static_cast<float>(level * level) * 0.85f + 0.5f);
    }
    static unsigned Duration(unsigned buff) {
        return buff == 717 ? 5000 : (buff == 718 || buff == 719) ? 10000 : 0;
    }
    // Literal base duration/count only. Traits464/477 and the unresolved fourth
    // CsValue327 parameter are not simulated; additional attributes are zero.
};
} // namespace hot49284
} // namespace glide
