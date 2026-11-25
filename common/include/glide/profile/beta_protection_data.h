#pragma once

namespace glide {
namespace beta {
struct Protection965 {
    enum : unsigned {
        Skill = 965,
        Buff = 280,
        OutcomePhase = 4,
        ActivationMs = 0,
        DurationMs = 3000,
        PulseMs = 3000,
        Pulses = 1,
        Radius = 170,
        Width = 340,
        LifetimeMs = 3000,
        MaximumAllies = 4
    };
    static constexpr bool Circle = true;
    static unsigned MaximumRange() {
        return 0;
    }
    static unsigned GrantDurationMs() {
        return DurationMs;
    }
    static constexpr bool Build(unsigned build) {
        return build == 14075 || build == 14422 || build == 14505 || build == 14812;
    }
};
} // namespace beta
} // namespace glide
