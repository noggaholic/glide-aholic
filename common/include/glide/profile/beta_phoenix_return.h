#pragma once
#include "glide/profile/beta_missile.h"
#include "glide/profile/beta_phoenix_data.h"
#include "glide/profile/beta_conditions.h"
#include "glide/profile/beta_health.h"
namespace glide {
namespace beta {
struct PhoenixReturn {
    bool returning = false;
    uint64_t limit = 0, lastRetarget = 0;
    enum Progress { Invalid = -1, Waiting = 0, Moved = 1, Arrived = 2, Cancelled = 3 };
    bool Due(const MissileFlight& f, const WorldActionContext& c) const {
        return c.persistCombatConditions && f.skill == Phoenix5258::Skill && f.phase == 2 &&
               (returning || c.now >= f.at);
    }
    Progress Advance(unsigned build, uint16_t owner, MissileFlight& f, const WorldActionContext& c, Buffer& bytes) {
        if (!Due(f, c))
            return Waiting;
        const std::array<float, 3> target{{c.position.x * 32, c.position.y * 32, -c.position.z * 32}};
        const auto position = returning ? (c.now < f.at ? f.PositionAt(c.now) : f.destination) : f.destination;
        if (!MissilePoint(target) || !MissilePoint(position) || !c.now)
            return Invalid;
        if (returning && c.now < lastRetarget)
            return Invalid;
        if (returning && c.now >= limit)
            return Cancelled;
        double distance = 0, shift = 0;
        for (size_t i = 0; i < 3; ++i) {
            distance += std::pow(double(target[i]) - position[i], 2);
            shift += std::pow(double(target[i]) - f.destination[i], 2);
        }
        distance = std::sqrt(distance);
        if (!std::isfinite(distance) || distance > double(Phoenix5258::Speed) * Phoenix5258::ReturnLimitMs / 1000)
            return Cancelled;
        if (distance <= Phoenix5258::ContactRadius)
            return Arrived;
        if (returning && c.now < f.at && (c.now - lastRetarget < Phoenix5258::RetargetMs || shift < 256))
            return Waiting;
        const auto travel = static_cast<uint64_t>(std::ceil(distance * 1000 / Phoenix5258::Speed));
        if (!travel || travel > Phoenix5258::ReturnLimitMs)
            return Cancelled;
        if (c.now > UINT64_MAX - Phoenix5258::ReturnLimitMs || c.sequence == UINT32_MAX)
            return Invalid;
        if (!MoveMissile(build, f.Agent(owner), c.sequence, f.identity, 2, owner, target, f.token, Phoenix5258::Speed,
                         bytes))
            return Invalid;
        if (!returning) {
            returning = true;
            limit = c.now + Phoenix5258::ReturnLimitMs;
        }
        lastRetarget = c.now;
        f.mode = 2;
        f.speed = Phoenix5258::Speed;
        f.start = position;
        f.destination = target;
        f.launchedAt = c.now;
        f.at = c.now + travel;
        return Moved;
    }
};
// 1 waiting,2 emitted/committed,0 save declined,-1 invalid. Prepare and encode on
//  copies, reserve before storage, then publish both ledgers and flight together.
template <class Effects>
int AdvancePhoenix(unsigned build, uint16_t owner, unsigned level, MissileFlight& live, PhoenixReturn& journey,
                   Effects& effects, Conditions& conditions, uint64_t& version, const std::string& recharges,
                   const WorldActionContext& c, WorldActionOutput& out) {
    auto flight = live;
    auto returning = journey;
    Buffer bytes;
    auto progress = returning.Advance(build, owner, flight, c, bytes);
    if (progress == PhoenixReturn::Invalid)
        return -1;
    if (progress == PhoenixReturn::Waiting)
        return 1;
    if (version == UINT64_MAX)
        return -1;
    auto pendingEffects = effects;
    Conditions::Prepared pendingConditions;
    WorldActionOutput result;
    result.sequences = 1;
    if (progress == PhoenixReturn::Arrived) {
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health, BetaHealthMaximum(build)))
            return -1;
        if (health <= 0)
            progress = PhoenixReturn::Cancelled;
    }
    if (progress == PhoenixReturn::Arrived) {
        if (!conditions.Prepare(c, modern::ConditionState<14505>::MaximumEntries, level, pendingConditions, result) ||
            !pendingEffects.StartPersonal(owner, Phoenix5258::Skill, c, level, bytes) ||
            !result.owned.Append(bytes.View()) || !result.peers.Append(bytes.View()))
            return -1;
    }
    if (progress == PhoenixReturn::Moved) {
        if (!result.owned.Append(bytes.View()) || !result.peers.Append(bytes.View()))
            return -1;
    } else {
        Buffer remove;
        if (!DestroyMissile(build, flight.Agent(owner), c.sequence, remove) || !result.owned.Append(remove.View()) ||
            !result.peers.Append(remove.View()))
            return -1;
        flight.phase = 0;
        returning = {};
    }
    if (progress == PhoenixReturn::Arrived) {
        if (!c.ReserveSequences(1))
            return -1;
        if (!c.persistCombatConditions || !c.persistCombatConditions(recharges, pendingEffects.Text(), c.healthState,
                                                                     pendingConditions.state.Text())) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return 0;
        }
        effects = std::move(pendingEffects);
        conditions.Commit(std::move(pendingConditions));
    }
    live = std::move(flight);
    journey = std::move(returning);
    result.peerEffectVersion = ++version;
    out = std::move(result);
    return 2;
}
} // namespace beta
} // namespace glide
