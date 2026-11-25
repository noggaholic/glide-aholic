#pragma once
#include "glide/profile/world_actions.h"
#include <cmath>

namespace glide {
namespace modern {
// Recipient,64-unit approach margin and completion by teleport are explicit local policies, not recovered
// collision/navigation. Units=2 preserves the fact; one personal marker is published, with no invented attack
// consumption, unblockable combat, conditioned1187 or full execution.
template <unsigned Build> struct TargetApproach {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported approach dialect");
    static const unsigned Skill = 28231, Buff = 28395, Units = 2, Duration = 5000;
    static const unsigned Activation = 569, Animation = 1333, Range = 1200, Gap = 64;
    static unsigned Cost() {
        return Build == 49284 ? 20u : 30u;
    }
    static unsigned Recharge() {
        return Build == 49284 ? 0u : 5000u;
    }
    static bool Destination(const WorldActionContext& c, uint32_t target, bool water, CatalogPosition& out,
                            bool& relocate) {
        if (!target || target == c.peerAgent || (Build != 49284 && target == 57344) || !c.isVisibleCharacterTarget ||
            !c.isVisibleCharacterTarget(target) || !c.characterTargetPosition)
            return false;
        CatalogPosition point;
        if (!c.characterTargetPosition(target, point))
            return false;
        const double x = double(point.x) - c.position.x, y = double(point.y) - c.position.y,
                     z = double(point.z) - c.position.z;
        if (!std::isfinite(c.position.x) || !std::isfinite(c.position.y) || !std::isfinite(c.position.z) ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        const double full = std::sqrt(x * x + y * y + z * z) * 32;
        if (!std::isfinite(full) || full > Range)
            return false;
        const double distance = std::sqrt(x * x + y * y + (water ? z * z : 0)) * 32;
        if (distance <= Gap) {
            out = c.position;
            relocate = false;
            return true;
        }
        const double fraction = double(Gap) / distance;
        CatalogPosition destination{static_cast<float>(point.x - x * fraction),
                                    static_cast<float>(point.y - y * fraction),
                                    water ? static_cast<float>(point.z - z * fraction) : point.z};
        if (!std::isfinite(destination.x) || !std::isfinite(destination.y) || !std::isfinite(destination.z))
            return false;
        out = destination;
        relocate = true;
        return true;
    }
};
} // namespace modern
} // namespace glide
