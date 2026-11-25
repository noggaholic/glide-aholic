#pragma once

namespace glide {
namespace modern {
template <unsigned Build> struct DemonHealingData {
    static_assert(Build == 167409 || Build == 204489, "Unsupported demon healing dialect");
    enum : unsigned {
        Skill = 28219,
        Legend = Build == 167409 ? 17 : 18,
        Slot = 0,
        MinimumLevel = 1,
        ActivationMs = 950,
        AnimationMs = 1667,
        RecoveryMs = 500,
        RechargeMs = 30000,
        Might = 740,
        MightDurationMs = 8000,
        DarkAura = 39978,
        DarkAuraDurationMs = 3000
    };
    static int Base(unsigned level) {
        return level && level <= 80 ? 120 + static_cast<int>(static_cast<float>(level * level) * .7f + .5f) : 0;
    }
    static int PerCondition(unsigned level) {
        return level && level <= 80 ? 20 + static_cast<int>(static_cast<float>(level * level) * .09f + .5f) : 0;
    }
};
} // namespace modern
} // namespace glide
