#pragma once
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_missile.h"
#include "glide/profile/beta_judgment_data.h"
#include "glide/profile/beta_siren_data.h"
#include "glide/profile/segment_distance.h"
#include <set>

namespace glide {
namespace beta {
// Connection-local contact history. Copy with the flight; discard on a failed
// durable commit. It never survives a world departure or stores a callback.
class JudgmentContacts {
    uint64_t sampledAt_ = 0;
    bool sampled_ = false;
    std::set<std::pair<uint16_t, std::string>> seen_;

  public:
    bool Prepare(unsigned build, uint16_t owner, unsigned level, const MissileFlight& flight,
                 const WorldActionContext& c, const std::vector<WorldConditionActor>& allies,
                 std::vector<WorldHealthChange>& targets, uint32_t selected = 0) {
        const bool siren = SirenHealingBuild(build) && flight.skill == Siren4606::Skill;
        if ((flight.skill != Judgment4522::Skill && !siren) || flight.phase != 2)
            return true;
        if (siren && (!selected || selected == owner))
            return true;
        if (!BetaHealthBuild(build) || !Judgment4522::Amount(level) || flight.at <= flight.launchedAt ||
            c.now < flight.launchedAt)
            return false;
        const auto endAt = (std::min)(c.now, flight.at);
        const auto beginAt = sampled_ ? sampledAt_ : flight.launchedAt;
        if (sampled_ && endAt <= beginAt)
            return true;
        const auto first = flight.PositionAt(beginAt), last = flight.PositionAt(endAt);
        const std::array<double, 3> start{{first[0], first[1], first[2]}}, end{{last[0], last[1], last[2]}};
        const float maximum = BetaHealthMaximum(build),
                    amount = siren ? SirenHealingAmount() : Judgment4522::Amount(level);
        bool complete = true;
        std::set<uint16_t> agents;
        for (const auto& actor : allies) {
            if (!actor.agent || !agents.insert(actor.agent).second || actor.identity.empty())
                return false;
            if (siren && actor.agent != selected)
                continue;
            if (actor.agent == owner || seen_.count({actor.agent, actor.identity}))
                continue;
            const std::array<double, 3> point{
                {double(actor.position.x) * 32, double(actor.position.y) * 32, -double(actor.position.z) * 32}};
            double distance = 0;
            if (!SegmentDistanceSquared(point, start, end, distance))
                return false;
            if (distance > double(Judgment4522::ContactRadius) * Judgment4522::ContactRadius)
                continue;
            float before = 0;
            if (!modern::ReadHealthState(actor.health, before, maximum))
                return false;
            if (before > 0 && before < maximum) {
                if (targets.size() == Judgment4522::BatchLimit) {
                    complete = false;
                    continue;
                }
                std::string health;
                if (!modern::WriteHealthState((std::min)(maximum, before + amount), health, maximum))
                    return false;
                targets.push_back({actor.agent, actor.identity, actor.health, health});
            }
            // Full/dead recipients have also been crossed. A later injury or
            // revival must not turn an old contact into another opportunity.
            seen_.insert({actor.agent, actor.identity});
        }
        // A wire batch limit is not a recipient cap. Keep this segment until
        // every crossed recipient has been processed on subsequent ticks.
        if (complete) {
            sampled_ = true;
            sampledAt_ = endAt;
        }
        return true;
    }
};

// 2 committed,1 no health publication,0 retry,-1 fatal. Always call on a copy.
template <class Shots>
int AdvanceJudgmentHealing(unsigned build, uint16_t owner, unsigned level, Shots& shots, uint64_t& version,
                           bool& recovery, const WorldActionContext& c, WorldActionOutput& out) {
    bool eligible = false;
    for (const auto& shot : shots)
        if (shot.flight.phase == 2 &&
            (shot.flight.skill == Judgment4522::Skill ||
             (SirenHealingBuild(build) && shot.flight.skill == Siren4606::Skill && shot.target)))
            eligible = true;
    if (!eligible)
        return 1;
    std::vector<WorldConditionActor> allies;
    if (c.conditionAllies && !c.conditionAllies(allies))
        return -1;
    for (auto& shot : shots) {
        std::vector<WorldHealthChange> targets;
        if (!shot.healing.Prepare(build, owner, level, shot.flight, c, allies, targets, shot.target))
            return -1;
        if (targets.empty())
            continue;
        if (version == UINT64_MAX || !c.persistAlliedSkillHealth || c.sequence > UINT32_MAX - targets.size())
            return -1;
        WorldActionOutput result;
        result.sequences = static_cast<uint32_t>(targets.size());
        uint32_t sequence = c.sequence;
        for (const auto& target : targets) {
            Buffer hit;
            if (!BetaMissileHit(build, shot.flight.Agent(owner), sequence++, shot.flight.identity, target.agent, hit) ||
                !result.owned.Append(hit.View()) || !result.peers.Append(hit.View()))
                return -1;
        }
        if (!c.ReserveSequences(result.sequences))
            return -1;
        const auto saved = c.persistAlliedSkillHealth(shot.flight.skill, c.healthState, targets);
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
