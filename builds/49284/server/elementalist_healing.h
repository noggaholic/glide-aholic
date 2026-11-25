#pragma once

namespace glide {
namespace hot49284 {
// The first own numeric model matches the active modern route; selecting it as the personal active heal is
// explicit compatibility policy. Do not add the second202 model or invent its passive event. No 5579 fact is
// present.
struct ElementalistHealing {
    static const unsigned Skill = 5503, Palette = 116, Slot = 0, Recharge = 25000;
    static unsigned Activation(bool water) {
        return water ? 1100 : 1350;
    }
    static unsigned Animation(bool water) {
        return water ? 2333 : 2067;
    }
    static int Amount(unsigned level) {
        return 75 + static_cast<int>(static_cast<float>(level * level) * 0.5f + 0.5f);
    }
};
} // namespace hot49284
} // namespace glide
