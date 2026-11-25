#pragma once
#include "glide/profile/modern_conditions.h"
#include "glide/profile/modern_health.h"
#include <cmath>
namespace glide {
namespace combat {
// Compatible single radial strike, not recovered original pulse execution.
// HoT own facts737x1/4s,722x1/3s,742x1/4s. Actual own named parameters:
// 737x2/4s,722x1/3s,742x1/5s. China uses that explicit compatible fallback.
// Actual 240 radius/5 target scalars; compatible geometry in HoT/China.
// Fixed nominal deadline, never renew on retry.
struct ElementalBlast {
    enum : unsigned { Skill = 27162, Radius = 240, Targets = 5 };
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static unsigned Legend(unsigned build) {
        return build == 49284 ? 11 : build == 167409 ? 14 : 15;
    }
    static unsigned Activation(unsigned build, bool water) {
        return build != 49284 && water ? 550 : 700;
    }
    static bool Contains(const CatalogPosition& origin, const CatalogPosition& point) {
        const double d = 32 * std::hypot(std::hypot(double(point.x) - origin.x, double(point.y) - origin.y),
                                         double(point.z) - origin.z);
        return std::isfinite(d) && d <= Radius;
    }
    template <unsigned Dialect>
    static bool Conditions(const WorldActionContext& c, uint64_t nominal, std::string& out) {
        modern::ConditionState<Dialect> state;
        if (!Build(Dialect) || !nominal || nominal > c.wallNow || nominal > UINT64_MAX - 5000 ||
            !state.Load(c.conditionState))
            return false;
        state.Expire(c.wallNow);
        const unsigned definitions[] = {737, 722, 742};
        const unsigned durations[] = {4000, 3000, Dialect == 49284 ? 4000u : 5000u};
        for (unsigned i = 0; i < 3; ++i)
            if (nominal + durations[i] > c.wallNow) {
                const unsigned count = i == 0 && Dialect != 49284 ? 2u : 1u;
                for (unsigned n = 0; n < count; ++n)
                    if (!state.Add(definitions[i], nominal, durations[i]))
                        return false;
            }
        out = state.Text();
        return true;
    }
};
} // namespace combat
} // namespace glide
