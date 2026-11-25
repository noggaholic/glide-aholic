#pragma once
#include "glide/profile/modern_conditions.h"
namespace glide {
namespace beta {
// Same independently verified condition IDs in all four betas; the controller
// supplies its own create/remove dialect, including April258/259.
using Conditions = modern::ConditionActions<14505>;
template <class Effects>
int SyncConditions(Conditions& live, unsigned level, uint64_t& version, const std::string& recharges,
                   const Effects& effects, const WorldActionContext& c, WorldActionOutput& out) {
    Conditions::Prepared next;
    WorldActionOutput result;
    result.sequences = 0;
    if (!live.Prepare(c, 0, level, next, result))
        return -1;
    if (!next.save && !next.changed)
        return 1;
    if (next.changed && version == UINT64_MAX)
        return -1;
    if (next.save) {
        if (!c.ReserveSequences(0))
            return -1;
        if (!c.persistCombatConditions ||
            !c.persistCombatConditions(recharges, effects.Text(), c.healthState, next.state.Text())) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return 0;
        }
    }
    live.Commit(std::move(next));
    if (next.changed)
        result.peerEffectVersion = ++version;
    out = std::move(result);
    return 2;
}
} // namespace beta
} // namespace glide
