#pragma once
#include "glide/profile/modern_health.h"

namespace glide {
namespace beta {
constexpr float kJuneMaximumHealth = 245.0f;
constexpr float kJulyMaximumHealth = 100.0f;
inline bool LegacyHealthBuild(unsigned build) {
    return build == 14075 || build == 14422 || build == 14505;
}
inline bool BetaHealthBuild(unsigned build) {
    return LegacyHealthBuild(build) || build == 14812;
}
inline float BetaHealthMaximum(unsigned build) {
    return build == 14812 ? kJulyMaximumHealth : kJuneMaximumHealth;
}
inline bool BetaHealthUpdate(unsigned build, uint16_t agent, float current, Buffer& out) {
    std::string checked;
    if (!BetaHealthBuild(build) || !agent || !modern::WriteHealthState(current, checked, BetaHealthMaximum(build)))
        return false;
    // Own receivers verified independently: April86/83, June and July108/105.
    // Absolute current,
    // with zero autonomous change for server-owned, persisted health.
    return out.U16(build == 14075 ? 86 : 108) && out.VarU32(agent) && out.F32(0) &&
           out.U16(build == 14075 ? 83 : 105) && out.VarU32(agent) && out.F32(current);
}
} // namespace beta
} // namespace glide
