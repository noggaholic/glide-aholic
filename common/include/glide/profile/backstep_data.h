#pragma once
#include <vector>

namespace glide {
namespace modern {
// Local compatibility: remove every active caster stack of these definitions at the activation event. The fact
// reference alone does not prove removal or recipient. Endurance50/25 uses the validated resource ledger. Accepted
// casts now reuse the local750ms DodgeEvasion policy; original timing/trait modifiers remain pending.
// China/Current explicitly select mode 4/flags1 boon725 fact (6s, no linked trait gate); HoT has no
// corresponding positive fact.
template <unsigned Build> struct BackstepData {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported backstep dialect");
    static const unsigned Skill = 29209, Activation = 800, Animation = 1000;
    static unsigned Cost() {
        return Build == 49284 ? 35u : 30u;
    }
    static unsigned Recharge() {
        return 0;
    }
    static unsigned EnduranceGain() {
        return Build == 49284 ? 50u : 25u;
    }
    static unsigned Boon() {
        return Build == 49284 ? 0u : 725u;
    }
    static unsigned BoonDuration() {
        return Build == 49284 ? 0u : 6000u;
    }
    static const std::vector<unsigned>& Definitions() {
        static const std::vector<unsigned> ids{721, 722, 727};
        return ids;
    }
};
} // namespace modern
} // namespace glide
