#pragma once

#include "glide/profile/regeneration.h"
#include "glide/profile/inspiring_field.h"
namespace glide {
namespace beta {
struct Symbol2527 {
    enum : unsigned {
        Skill = 2527,
        Buff = 289,
        ActivationMs = 900,
        PulseMs = 1000,
        Pulses = 4,
        DurationMs = 1000,
        Radius = 180,
        Width = 360,
        LifetimeMs = 4000,
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
using Regeneration = modern::RegenerationState<Symbol2527::Buff, true>;
using SymbolField = modern::AlliedField<Symbol2527>;
} // namespace beta
} // namespace glide
