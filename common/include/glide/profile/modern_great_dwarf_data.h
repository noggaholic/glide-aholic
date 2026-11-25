#pragma once
// Own effect and allied delivery share the authored duration. Direct-hit reduction uses the explicit local50%
// combat policy. Original attribute formula and conditional80245 remain unimplemented.
namespace glide {
namespace modern {
template <unsigned Build> struct GreatDwarfData {
    static_assert(Build == 167409 || Build == 204489, "Unsupported Great Dwarf dialect");
    enum : unsigned {
        Skill = 27975,
        Legend = Build == 167409 ? 16 : 17,
        Slot = 4,
        MinimumLevel = 1,
        EnergyCost = 40,
        ActivationMs = 1667,
        AnimationMs = 2000,
        RecoveryMs = 0,
        RechargeMs = 0,
        Buff = 26596,
        DurationMs = 5000
    };
};
} // namespace modern
} // namespace glide
