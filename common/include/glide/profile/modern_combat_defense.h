#pragma once
#include "glide/profile/regeneration.h"

namespace glide {
namespace modern {
// Compatibility policy: modern Protection717 uses the validated beta -33%
// direct-hit reduction. Rite26596 uses a local50% reduction in all three builds.
// No claim of recovered modern attribute calculations.
inline bool PositiveEffectRows(const std::string& effects, std::string& rows) {
    if (effects.size() > 8192)
        return false;
    rows = effects;
    // Hammers/elite/facet envelopes retain ordinary positive effects after '|'.
    // The owning profile validates the complete envelope at character load.
    if (rows.compare(0, 3, "H1:") == 0 || rows.compare(0, 3, "H2:") == 0 || rows.compare(0, 3, "H3:") == 0) {
        const auto bar = rows.find('|');
        if (bar == std::string::npos)
            return false;
        rows = rows.substr(bar + 1);
    }
    return true;
}
inline bool DefenseDamage(unsigned build, const std::string& effects, uint64_t utc, float& damage) {
    damage = 1.0f;
    if (effects.empty())
        return true;
    std::string rows;
    if (!PositiveEffectRows(effects, rows))
        return false;
    bool active = false;
    if (!Regeneration::StoredBuffActive(rows, 717, utc, build == 49284 ? 5000 : 3000, active))
        return false;
    if (active)
        damage = 0.67f;
    if (!Regeneration::StoredBuffActive(rows, 26596, utc, 5000, active))
        return false;
    if (active)
        damage *= 0.5f;
    return true;
}
} // namespace modern
} // namespace glide
