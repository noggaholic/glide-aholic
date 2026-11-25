#pragma once
#include "glide/profile/world_actions.h"
#include "glide/profile/modern_conditions.h"
#include "glide/profile/modern_health.h"
#include <algorithm>
#include <cmath>
#include <set>
namespace glide {
namespace combat {
// 27080 default facts:720/890 once for5s in all three builds.
// Radius600 and maximum5 form the explicit compatible area policy.
// Revealed890 is deliberately NOT inserted in the cleansable condition ledger.
struct GazeDarkness {
    enum : unsigned { Skill = 27080, Blind = 720, Reveal = 890, Duration = 5000, Radius = 600, MaximumTargets = 5 };
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static bool Deadline(uint64_t nominal, uint64_t utc, uint64_t& end) {
        if (!nominal || nominal > utc || nominal > UINT64_MAX - Duration)
            return false;
        end = nominal + Duration;
        return true;
    }
    template <unsigned Dialect>
    static bool Conditions(const WorldActionContext& c, uint64_t nominal, std::string& out) {
        if (!Build(Dialect))
            return false;
        uint64_t end = 0;
        modern::ConditionState<Dialect> state;
        if (!Deadline(nominal, c.wallNow, end) || !state.Load(c.conditionState))
            return false;
        state.Expire(c.wallNow);
        if (end > c.wallNow && !state.Add(Blind, nominal, Duration))
            return false;
        out = state.Text();
        return true;
    }
    static bool RevealEffect(uint64_t nominal, uint64_t utc, ReceivedWorldEffect& out) {
        out = {};
        uint64_t end = 0;
        if (!Deadline(nominal, utc, end))
            return false;
        if (end > utc) {
            out.definition = Reveal;
            out.endsUtc = end;
        }
        return true;
    }
    static WorldConditionCommit Commit(unsigned build, const WorldActionContext& c, const std::string& deadlines,
                                       const std::string& effects) {
        std::vector<WorldCombatTarget> selected;
        if (!c.persistHostileEffects || !Select(build, c, c.wallNow, selected))
            return WorldConditionCommit::NotApplied;
        std::vector<WorldHostileTarget> targets;
        for (const auto& enemy : selected) {
            WorldHostileTarget target;
            target.agent = enemy.actor.agent;
            target.identity = enemy.actor.identity;
            targets.push_back(std::move(target));
        }
        return c.persistHostileEffects(Skill, c.wallNow, deadlines, effects, targets);
    }
    static bool Select(unsigned build, const WorldActionContext& c, uint64_t nominal,
                       std::vector<WorldCombatTarget>& out) {
        out.clear();
        uint64_t end = 0;
        float own = 0;
        const float maximum = build == 49284 ? modern::kHotMaximumHealth : modern::kPublishedMaximumHealth;
        if (!Build(build) || !c.peerAgent || !Deadline(nominal, c.wallNow, end) ||
            !modern::ReadHealthState(c.healthState, own, maximum) || !std::isfinite(c.position.x) ||
            !std::isfinite(c.position.y) || !std::isfinite(c.position.z))
            return false;
        if (own <= 0 || end <= c.wallNow)
            return true;
        // The server supplies only live, mutually enabled opponents in this map.
        std::vector<WorldCombatTarget> candidates;
        if (!c.combatEnemies || !c.combatEnemies(candidates))
            return false;
        struct Row {
            WorldCombatTarget target;
            double distance = 0;
        };
        std::vector<Row> rows;
        std::set<unsigned> agents;
        std::set<std::string> identities;
        for (const auto& target : candidates) {
            const auto& actor = target.actor;
            float health = 0;
            if (!actor.agent || actor.identity.empty() || !agents.insert(actor.agent).second ||
                !identities.insert(actor.identity).second || !modern::ReadHealthState(actor.health, health, maximum))
                return false;
            const double distance = 32 * std::hypot(std::hypot(double(actor.position.x) - c.position.x,
                                                               double(actor.position.y) - c.position.y),
                                                    double(actor.position.z) - c.position.z);
            if (!std::isfinite(distance))
                return false;
            if (actor.agent == c.peerAgent || health <= 0 || distance > Radius)
                continue;
            Row row;
            row.target = target;
            row.distance = distance;
            rows.push_back(std::move(row));
        }
        std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
            return a.distance != b.distance ? a.distance < b.distance : a.target.actor.agent < b.target.actor.agent;
        });
        for (const auto& row : rows) {
            out.push_back(row.target);
            if (out.size() == MaximumTargets)
                break;
        }
        return true;
    }
};
} // namespace combat
} // namespace glide
