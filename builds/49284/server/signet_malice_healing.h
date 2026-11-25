#pragma once

namespace glide {
namespace hot49284 {
// Palette133/profession5; compatible initial assignment underwater. Use own aquatic marker1100 and stage2333.
// Trait1267/16536 is not a base benefit. Selecting the resolved75+.5*level^2 model for activation is explicit
// policy; the small132 description has no identified event and produces no pulses.
struct SignetMaliceHealing {
    static const unsigned Skill = 13050, Palette = 133, Slot = 0, Activation = 1100, Animation = 2333, Recharge = 15000;
    static int Amount(unsigned level) {
        return 75 + static_cast<int>(static_cast<float>(level * level) * .5f + .5f);
    }
};
} // namespace hot49284
} // namespace glide
