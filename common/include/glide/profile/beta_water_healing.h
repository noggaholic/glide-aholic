#pragma once
#include "glide/profile/beta_health.h"
#include "glide/profile/june_water_healing.h"
#include <array>
#include <set>

namespace glide {
namespace beta {
struct PreparedWaterHealing {
    std::string ownHealth;
    std::vector<WorldHealthChange> targets;
    std::vector<uint16_t> healedAgents;
    Buffer owned, peers;
};
// July's 63/radius180 match the retained June values. Target cap5 is the
// same explicit cooperative exploration policy, not a recovered server cap.
// Prepare copies only; the caller reserves sequences and commits atomically.
inline bool PrepareAreaHealing(unsigned build, uint16_t agent, const std::array<float, 3>& point,
                               const WorldActionContext& context, float amount, float radius, size_t limit,
                               PreparedWaterHealing& out, bool selfFirst = false, unsigned skill = 1155) {
    if ((!BetaHealthBuild(build) && build != 49284 && build != 167409 && build != 204489) || !agent ||
        !std::isfinite(amount) || amount <= 0 || !std::isfinite(radius) || radius <= 0 || !limit)
        return false;
    for (float value : point)
        if (!std::isfinite(value))
            return false;
    const float maximum = BetaHealthBuild(build) ? BetaHealthMaximum(build)
                          : build == 49284       ? modern::kHotMaximumHealth
                                                 : modern::kPublishedMaximumHealth;
    float current = 0;
    if (!modern::ReadHealthState(context.healthState, current, maximum))
        return false;
    struct Candidate {
        WorldConditionActor actor;
        double distance = 0;
        float health = 0;
        bool self = false;
    };
    std::vector<WorldConditionActor> allies;
    if (context.conditionAllies && !context.conditionAllies(allies))
        return false;
    WorldConditionActor self;
    self.agent = agent;
    self.identity = context.selfIdentity;
    self.position = context.position;
    self.health = context.healthState;
    allies.push_back(std::move(self));
    std::vector<Candidate> candidates;
    std::set<uint16_t> seen;
    for (const auto& actor : allies) {
        if (!actor.agent || !seen.insert(actor.agent).second)
            return false;
        float health = 0;
        if (!modern::ReadHealthState(actor.health, health, maximum))
            return false;
        if (health <= 0 || health >= maximum)
            continue;
        const double dx = double(actor.position.x) * 32 - point[0], dy = double(actor.position.y) * 32 - point[1],
                     dz = -double(actor.position.z) * 32 - point[2];
        const double distance = dx * dx + dy * dy + dz * dz;
        if (!std::isfinite(distance))
            return false;
        if (distance > double(radius) * radius)
            continue;
        candidates.push_back({actor, distance, health, actor.agent == agent});
    }
    std::sort(candidates.begin(), candidates.end(), [selfFirst](const Candidate& a, const Candidate& b) {
        if (selfFirst && a.self != b.self)
            return a.self;
        return a.distance != b.distance ? a.distance < b.distance : a.actor.agent < b.actor.agent;
    });
    if (candidates.size() > limit)
        candidates.resize(limit);
    PreparedWaterHealing prepared;
    prepared.ownHealth = context.healthState;
    for (const auto& candidate : candidates) {
        const float health = (std::min)(maximum, candidate.health + amount);
        std::string text;
        if (!modern::WriteHealthState(health, text, maximum))
            return false;
        if (candidate.self) {
            prepared.ownHealth = text;
            bool encoded = false;
            if (BetaHealthBuild(build))
                encoded = BetaHealthUpdate(build, agent, health, prepared.owned) &&
                          BetaHealthUpdate(build, agent, health, prepared.peers);
            else if (build == 49284)
                encoded = modern::HealthDelta<49284>(agent, skill, health - candidate.health, agent, prepared.owned) &&
                          modern::HealthDelta<49284>(agent, skill, health - candidate.health, agent, prepared.peers);
            else if (build == 167409)
                encoded = modern::HealthDelta<167409>(57344, skill, health - candidate.health, 57344, prepared.owned) &&
                          modern::HealthDelta<167409>(agent, skill, health - candidate.health, agent, prepared.peers);
            else
                encoded = modern::HealthDelta<204489>(57344, skill, health - candidate.health, 57344, prepared.owned) &&
                          modern::HealthDelta<204489>(agent, skill, health - candidate.health, agent, prepared.peers);
            if (!encoded)
                return false;
        } else
            prepared.targets.push_back({candidate.actor.agent, candidate.actor.identity, candidate.actor.health, text});
        prepared.healedAgents.push_back(candidate.actor.agent);
    }
    out = std::move(prepared);
    return true;
}
inline bool PrepareWaterHealing(unsigned build, uint16_t agent, const std::array<float, 3>& point,
                                const WorldActionContext& context, PreparedWaterHealing& out) {
    return PrepareAreaHealing(build, agent, point, context, WaterGroundHealing::Amount, WaterGroundHealing::Radius,
                              WaterGroundHealing::Limit, out);
}
} // namespace beta
} // namespace glide
