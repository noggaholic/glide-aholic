#pragma once
#include "glide/profile/modern_conditions.h"

namespace glide {
namespace combat {
// Compatible policy for the supported modern attacks: one contribution per
// resolved strike, including an area strike. No target/contact consumes none.
// Blindness is resolved before block/evasion; it grants no on-hit benefits.
struct Blindness {
    struct Prepared {
        unsigned instance = 0;
        std::string after;
    };
    template <unsigned Build> static bool Prepare(const std::string& text, uint64_t utc, Prepared& out) {
        modern::ConditionState<Build> state;
        if (!state.Load(text))
            return false;
        out = {};
        out.after = text;
        if (state.Entries().empty())
            return true;
        if (!utc)
            return false;
        out.instance = state.ConsumeOne(720, utc);
        if (out.instance)
            out.after = state.Text();
        return true;
    }
    static bool Prepare(unsigned build, const std::string& text, uint64_t utc, Prepared& out) {
        switch (build) {
        case 49284:
            return Prepare<49284>(text, utc, out);
        case 167409:
            return Prepare<167409>(text, utc, out);
        case 204489:
            return Prepare<204489>(text, utc, out);
        default:
            out = {};
            out.after = text;
            return true;
        }
    }
    static bool Active(unsigned build, const WorldActionContext& c, bool& active) {
        Prepared prepared;
        if (!Prepare(build, c.conditionState, c.wallNow, prepared))
            return false;
        active = prepared.instance != 0;
        return true;
    }
};
} // namespace combat
} // namespace glide
