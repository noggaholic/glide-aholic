#pragma once

namespace glide {
namespace modern {
// Build 204489: initial-support-skill-{actions,phases,quantities,parameters}.txt. Selecting the 75 + .5*level^2 model
// for activation is explicit compatibility policy. The small description model has no identified event; do not
// pulse it.
template <unsigned Build> struct SignetMaliceHealing {
    static_assert(Build == 167409 || Build == 204489, "Unsupported signet build");
    enum : unsigned {
        Skill = 13050,
        Palette = 133,
        MinimumLevel = 1,
        Recharge = 12000,
        Activation = Build == 167409 ? 950 : 700,
        Animation = Build == 167409 ? 1667 : 1500
    };
    static int Amount(unsigned level) {
        return 75 + static_cast<int>(level * level * .5f + .5f);
    }
};
} // namespace modern
} // namespace glide
