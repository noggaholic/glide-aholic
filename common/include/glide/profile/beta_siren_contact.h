#pragma once
#include "glide/profile/beta_siren_data.h"
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_missile.h"
#include "glide/profile/segment_distance.h"

namespace glide {
namespace beta {
// Only the originally selected ally can receive this first-contact effect.
// This does not turn the orb into a piercing shot or invent enemy bounces.
class SirenContact {
    uint64_t sampledAt_ = 0;
    bool sampled_ = false, delivered_ = false;

  public:
    bool Prepare(uint16_t owner, uint32_t target, const MissileFlight& flight, const WorldActionContext& c,
                 const std::vector<WorldConditionActor>& allies, std::vector<WorldReceivedEffectTarget>& recipients) {
        if (delivered_ || !target || target == owner || flight.skill != Siren4606::Skill || flight.phase != 2)
            return true;
        if (!flight.identity || flight.at <= flight.launchedAt || c.now < flight.launchedAt)
            return false;
        const auto endAt = (std::min)(c.now, flight.at), beginAt = sampled_ ? sampledAt_ : flight.launchedAt;
        if (sampled_ && endAt <= beginAt)
            return true;
        const auto first = flight.PositionAt(beginAt), last = flight.PositionAt(endAt);
        const std::array<double, 3> start{{first[0], first[1], first[2]}}, end{{last[0], last[1], last[2]}};
        const WorldConditionActor* selected = nullptr;
        for (const auto& actor : allies)
            if (actor.agent == target) {
                if (selected)
                    return false;
                selected = &actor;
            }
        if (selected) {
            if (selected->identity.empty())
                return false;
            const std::array<double, 3> point{{double(selected->position.x) * 32, double(selected->position.y) * 32,
                                               -double(selected->position.z) * 32}};
            double distance = 0;
            if (!SegmentDistanceSquared(point, start, end, distance))
                return false;
            if (distance <= double(Siren4606::ContactRadius) * Siren4606::ContactRadius) {
                float health = 0;
                if (!modern::ReadHealthState(selected->health, health, BetaHealthMaximum(14812)))
                    return false;
                if (health > 0) {
                    const auto definition = Siren4606::Definition(flight.identity),
                               duration = Siren4606::Duration(definition);
                    if (!c.wallNow || c.wallNow > UINT64_MAX - duration)
                        return false;
                    WorldReceivedEffectTarget request;
                    request.agent = selected->agent;
                    request.identity = selected->identity;
                    request.definition = definition;
                    request.endsUtc = c.wallNow + duration;
                    recipients.push_back(request);
                }
                delivered_ = true;
            }
        }
        sampled_ = true;
        sampledAt_ = endAt;
        return true;
    }
};

// Always call on a disposable controller copy.2 committed,1 unchanged,0 retry,-1 fatal.
template <class Shots>
int AdvanceSirenContact(uint16_t owner, Shots& shots, uint64_t& version, bool& recovery, const std::string& deadlines,
                        const WorldActionContext& c, WorldActionOutput& out) {
    bool eligible = false;
    for (const auto& shot : shots)
        if (shot.flight.skill == Siren4606::Skill && shot.flight.phase == 2 && shot.target)
            eligible = true;
    if (!eligible)
        return 1;
    std::vector<WorldConditionActor> allies;
    if (c.conditionAllies && !c.conditionAllies(allies))
        return -1;
    for (auto& shot : shots) {
        std::vector<WorldReceivedEffectTarget> recipients;
        if (!shot.siren.Prepare(owner, shot.target, shot.flight, c, allies, recipients))
            return -1;
        if (recipients.empty())
            continue;
        if (recipients.size() != 1 || !c.persistPreparedAlliedEffects || version == UINT64_MAX)
            return -1;
        WorldActionOutput result;
        result.sequences = 1;
        if (!BetaMissileHit(14812, shot.flight.Agent(owner), c.sequence, shot.flight.identity, recipients[0].agent,
                            result.owned) ||
            !result.peers.Append(result.owned.View()) || !c.ReserveSequences(1))
            return -1;
        std::vector<unsigned> allocated;
        const auto saved = c.persistPreparedAlliedEffects(
            deadlines,
            [&](const std::vector<unsigned>& ids, std::string& own) {
                if (ids.size() != 1 || !ids[0])
                    return false;
                own = c.skillEffects;
                return true;
            },
            recipients, allocated);
        if (saved == WorldConditionCommit::RecoveryRequired) {
            recovery = true;
            return -1;
        }
        if (saved != WorldConditionCommit::Applied)
            return 0;
        result.peerEffectVersion = ++version;
        out = std::move(result);
        return 2;
    }
    return 1;
}
} // namespace beta
} // namespace glide
