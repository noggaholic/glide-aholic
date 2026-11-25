#pragma once
#include "glide/profile/beta_health.h"

namespace glide {
namespace beta {
// Keep the maximum already used by the accepted June bootstrap. Full level,
// profession and equipment scaling is outside this exploration health model.
inline bool JuneHealthBuild(unsigned build) {
    return build == 14422 || build == 14505;
}
inline bool JuneHealthUpdate(unsigned build, uint16_t agent, float current, Buffer& out) {
    return JuneHealthBuild(build) && BetaHealthUpdate(build, agent, current, out);
}
} // namespace beta
} // namespace glide
