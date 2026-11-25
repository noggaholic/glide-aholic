#pragma once
#include "glide/profile/modern_conditions.h"
#include "glide/profile/segment_distance.h"
#include "glide/profile/beta_judgment_data.h"
namespace glide {
namespace combat {

struct BanishEnchantment {
    enum : unsigned { Skill = 27505, Activation = 620, Animation = 1350, Cost = 20 };
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static unsigned Range(unsigned build) {
        return build == 49284 ? 600 : 900;
    }
    static unsigned Legend(unsigned build) {
        return build == 49284 ? 14 : build == 167409 ? 17 : 18;
    }
    static unsigned RemovalLimit(unsigned build) {
        return build == 49284 ? 2 : 3;
    }
    static bool Contains(unsigned build, const CatalogPosition& origin, const CatalogPosition& aim,
                         const CatalogPosition& point) {
        const double dx = double(aim.x) - origin.x, dy = double(aim.y) - origin.y, dz = double(aim.z) - origin.z,
                     length = std::hypot(std::hypot(dx, dy), dz);
        const double scale = length ? Range(build) / length : 0;
        const std::array<double, 3> a{{double(origin.x) * 32, double(origin.y) * 32, double(origin.z) * 32}},
            z{{a[0] + (length ? dx * scale : Range(build)), a[1] + dy * scale, a[2] + dz * scale}},
            p{{double(point.x) * 32, double(point.y) * 32, double(point.z) * 32}};
        double distance = 0;
        const auto range = double(Range(build));
        return std::hypot(std::hypot(p[0] - a[0], p[1] - a[1]), p[2] - a[2]) <= range &&
               SegmentDistanceSquared(p, a, z, distance) &&
               distance <= double(beta::Judgment4522::ContactRadius) * beta::Judgment4522::ContactRadius;
    }
    template <unsigned Build>
    static bool Conditions(const WorldActionContext& context, uint64_t nominal, std::string& text) {
        modern::ConditionState<Build> state;
        if (!nominal || nominal > context.wallNow || !state.Load(context.conditionState))
            return false;
        state.Expire(context.wallNow);
        const auto add = [&](unsigned def, unsigned count, unsigned duration) {
            if (nominal > UINT64_MAX - duration)
                return false;
            if (nominal + duration <= context.wallNow)
                return true;
            for (unsigned i = 0; i < count; ++i)
                if (!state.Add(def, nominal, duration))
                    return false;
            return true;
        };
        bool hot = Build == 49284, current = Build == 204489;
        if (hot) {
            if (!add(861, 2, 6000) || !add(738, 3, 6000))
                return false;
        } else {
            if (!add(722, 1, 3000))
                return false;
            // Three 1s contributions stack duration.
            if (current && !add(21632, 1, 3000))
                return false;
        }
        text = state.Text();
        return true;
    }
};
} // namespace combat
} // namespace glide
