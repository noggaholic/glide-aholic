#pragma once
#include "glide/profile/demon_leap_data.h"
#include "glide/profile/modern_conditions.h"
namespace glide {
namespace combat {
// Enemy contact at the authored activation after the existing movement.
// Radius360/max5 and one contact are explicit compatible policies. HoT uses
// one 3-stack21632 row, not the sum of two unassigned identical facts.
// Neither original periodic fields nor conditional trait26980 are inferred.
struct DemonLeapImpact {
    enum : unsigned { Skill = 27917, Radius = 360, Targets = 5 };
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static unsigned Legend(unsigned build) {
        return build == 49284 ? 14 : build == 167409 ? 17 : 18;
    }
    static unsigned Activation(unsigned build) {
        return build == 49284 ? 1000 : 760;
    }
    static bool Contains(const CatalogPosition& origin, const CatalogPosition& point) {
        const double d = 32 * std::hypot(std::hypot(double(point.x) - origin.x, double(point.y) - origin.y),
                                         double(point.z) - origin.z);
        return std::isfinite(d) && d <= Radius;
    }
    template <unsigned Dialect>
    static bool Conditions(const WorldActionContext& c, uint64_t nominal, std::string& out) {
        modern::ConditionState<Dialect> state;
        bool hot = Dialect == 49284;
        const unsigned duration = hot ? 5000 : 2000;
        if (!Build(Dialect) || !nominal || nominal > c.wallNow || nominal > UINT64_MAX - duration ||
            !state.Load(c.conditionState))
            return false;
        state.Expire(c.wallNow);
        if (nominal + duration > c.wallNow) {
            if (!state.Add(Dialect == 49284 ? 721 : 722, nominal, duration))
                return false;
            if (hot)
                for (unsigned n = 0; n < 3; ++n)
                    if (!state.Add(21632, nominal, duration))
                        return false;
        }
        out = state.Text();
        return true;
    }
};
} // namespace combat
} // namespace glide
