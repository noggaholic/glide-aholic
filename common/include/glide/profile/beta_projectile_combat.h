#pragma once
#include "glide/profile/projectile_combat.h"
#include "glide/profile/beta_missile.h"

namespace glide {
namespace beta {
template <class Shots> void KeepProjectileSamples(Shots& live, const Shots& proposed) {
    for (size_t i = 0; i < live.size(); ++i)
        if (live[i].flight.phase == 2 && proposed[i].flight.phase == 2 &&
            live[i].flight.skill == proposed[i].flight.skill && live[i].flight.identity == proposed[i].flight.identity)
            live[i].contact = proposed[i].contact;
}
// Same proposal/commit convention as existing allied projectile contacts.
template <class Shots>
int AdvanceProjectileCombat(unsigned build, uint16_t owner, Shots& shots, uint64_t& version, bool& recovery,
                            const WorldActionContext& c, WorldActionOutput& out) {
    for (auto& shot : shots) {
        auto& flight = shot.flight;
        if (flight.phase != 2 || !shot.contact.skill)
            continue;
        auto contact = shot.contact;
        WorldHealthChange enemy;
        const int hit = contact.Sample(build, c, flight.launchedAt, flight.at, flight.start, flight.destination,
                                       shot.contactLift, enemy);
        if (hit < 0)
            return -1;
        if (!hit) {
            shot.contact = std::move(contact);
            continue;
        }
        if (version == UINT64_MAX || c.sequence > UINT32_MAX - 2 || !c.persistCombatHit)
            return -1;
        WorldActionOutput result;
        result.sequences = c.now >= flight.at ? 2u : 1u;
        Buffer retirement;
        if (!BetaMissileHit(build, flight.Agent(owner), c.sequence, flight.identity, enemy.agent, result.owned) ||
            (c.now >= flight.at && !DestroyMissile(build, flight.Agent(owner), c.sequence + 1, retirement)) ||
            !result.owned.Append(retirement.View()) || !result.peers.Append(result.owned.View()) ||
            !c.ReserveSequences(result.sequences))
            return -1;
        const auto saved = c.persistCombatHit(flight.skill, c.healthState, enemy);
        if (saved == WorldConditionCommit::RecoveryRequired) {
            recovery = true;
            return -1;
        }
        if (saved != WorldConditionCommit::Applied)
            return 0;
        if (c.now >= flight.at)
            flight.phase = 0;
        shot.contact = {};
        result.peerEffectVersion = ++version;
        out = std::move(result);
        return 2;
    }
    return 1;
}
} // namespace beta
} // namespace glide
