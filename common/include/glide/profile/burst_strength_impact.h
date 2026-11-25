#pragma once
#include "glide/profile/elemental_blast.h"
#include "glide/profile/dragon_facet_data.h"
namespace glide {
namespace combat {
// One compatible radial contact. Own range300; max5 and single contact are
// explicit compatible policies. HoT 738x10/6000; no modern vulnerability
// inferred from the different modern personal51653 effect.
struct BurstStrengthImpact {
    enum : unsigned { Skill = 28113, Radius = 300, Targets = 5 };
    static bool Build(unsigned build) {
        return ElementalBlast::Build(build);
    }
    static unsigned Legend(unsigned build) {
        return ElementalBlast::Legend(build);
    }
    static unsigned Activation(unsigned build, bool water) {
        return build != 49284 && water ? 1150 : 1200;
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
        if (!Build(Dialect) || !nominal || nominal > c.wallNow || nominal > UINT64_MAX - 6000 ||
            !state.Load(c.conditionState))
            return false;
        state.Expire(c.wallNow);
        if (hot && nominal + 6000 > c.wallNow)
            for (unsigned n = 0; n < 10; ++n)
                if (!state.Add(738, nominal, 6000))
                    return false;
        out = state.Text();
        return true;
    }
};
} // namespace combat
} // namespace glide
