#pragma once
#include "glide/profile/world_actions.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace glide {
namespace modern {
// Ground-point orientation and the distance cap are explicit local policies. No teleport, collision guarantee,
// enemy hit or condition recipient is inferred.
template <unsigned Build> struct DemonLeapData {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported leap dialect");
    enum : unsigned {
        Skill = 27917,
        Range = 600,
        Animation = 1931,
        MoveStart = 80,
        MoveEnd = 680,
        Speed = 1000,
        Activation = Build == 49284 ? 1000 : 760
    };
    static unsigned Cost() {
        return Build == 49284 ? 35u : 30u;
    }
    static unsigned Recharge() {
        return Build == 49284 ? 0u : 3000u;
    }
    static bool ValidPoint(const WorldActionContext& context, const CatalogPosition& destination) {
        const double x = (double(destination.x) - context.position.x) * 32;
        const double y = (double(destination.y) - context.position.y) * 32;
        const double z = (double(destination.z) - context.position.z) * 32;
        const double squared = x * x + y * y + z * z;
        return std::isfinite(squared) && squared <= double(Range) * Range;
    }
    static bool Aim(const WorldActionContext& context, const CatalogPosition& destination, bool water,
                    unsigned remainingMs, std::array<float, 3>& direction, float& speed, bool& moving) {
        if (!remainingMs || !ValidPoint(context, destination))
            return false;
        const double x = double(destination.x) - context.position.x, y = double(destination.y) - context.position.y;
        const double z = water ? double(destination.z) - context.position.z : 0;
        const double length = std::sqrt(x * x + y * y + z * z);
        direction = {{1, 0, 0}};
        speed = 0;
        moving = false;
        if (length * 32 <= .001)
            return true;
        direction = {{float(x / length), float(y / length), float(z / length)}};
        speed = float((std::min)(double(Speed), length * 32000 / remainingMs));
        // Own movement wire requires a speed of at least one native unit/s.
        moving = speed >= 1;
        return true;
    }
};
} // namespace modern
} // namespace glide
