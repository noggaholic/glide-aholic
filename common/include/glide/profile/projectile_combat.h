#include "glide/profile/blindness.h"
#include "glide/profile/infuse_light.h"
#pragma once
#include "glide/profile/beta_combat_defense.h"
#include "glide/profile/combat_damage.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_judgment_data.h"
#include "glide/profile/segment_distance.h"

namespace glide {
namespace combat {
// Existing own weapon-flight registrations; no new resource or wire schema.
inline bool ProjectileSkill(unsigned build, unsigned skill) {
    if (beta::BetaHealthBuild(build)) {
        switch (skill) {
        case 437:
        case 1006:
        case 1487:
        case 2674:
        case 2676:
        case 2782:
        case 2907:
        case 2960:
        case 2961:
        case 3218:
        case 3220:
        case 3221:
        case 4165:
        case 4310:
        case 4330:
        case 4522:
        case 4539:
        case 4606:
        case 4710:
        case 6225:
        case 13417:
            return true;
        case 6222:
            return build == 14075;
        case 4166:
            return build != 14075;
        default:
            return false;
        }
    }
    if (build != 49284 && build != 167409 && build != 204489)
        return false;
    switch (skill) {
    case 12510:
    case 5652:
    case 13070:
    case 28692:
    case 28827:
        return true;
    case 5605:
    case 5941:
    case 6147:
    case 5879:
    case 6145:
        return build == 49284;
    default:
        return false;
    }
}
// Reuse the existing beta contact radius/segment method as an explicit local
// policy for selected-target weapon projectiles. No terrain collision or homing.
constexpr double ProjectileContactRadius = beta::Judgment4522::ContactRadius;
struct ProjectileContact {
    unsigned skill = 0;
    uint32_t target = 0;
    std::string identity;
    uint64_t sampledAt = 0;
    bool sampled = false;
    void Capture(unsigned build, unsigned ability, uint32_t selected, const WorldActionContext& c) {
        *this = {};
        WorldCombatTarget enemy;
        if (ProjectileSkill(build, ability) && selected && c.combatTarget && c.combatTarget(selected, enemy)) {
            skill = ability;
            target = selected;
            identity = enemy.actor.identity;
        }
    }
    bool Matches(const WorldActionContext& c) const {
        if (!skill)
            return true;
        WorldCombatTarget enemy;
        return c.combatTarget && c.combatTarget(target, enemy) && enemy.actor.agent == target &&
               enemy.actor.identity == identity;
    }
    // Pure health proposal; call on a copy.1 hit,0 sampled/missed,-1 malformed.
    int Sample(unsigned build, const WorldActionContext& c, uint64_t launched, uint64_t arrives,
               const std::array<float, 3>& start, const std::array<float, 3>& end, float lift,
               WorldHealthChange& change) {
        if (!skill)
            return 0;
        if (arrives <= launched || c.now < launched)
            return -1;
        const auto first = sampled ? sampledAt : launched, last = (std::min)(c.now, arrives);
        if (last <= first)
            return 0;
        WorldCombatTarget enemy;
        if (!c.combatTarget || !c.combatTarget(target, enemy) || enemy.actor.agent != target ||
            enemy.actor.identity != identity) {
            *this = {};
            return 0;
        }
        const float maximum = beta::BetaHealthBuild(build) ? beta::BetaHealthMaximum(build)
                              : build == 49284             ? modern::kHotMaximumHealth
                                                           : modern::kPublishedMaximumHealth;
        float own = 0, health = 0;
        if (!modern::ReadHealthState(c.healthState, own, maximum) ||
            !modern::ReadHealthState(enemy.actor.health, health, maximum))
            return -1;
        if (own <= 0 || health <= 0) {
            *this = {};
            return 0;
        }
        std::array<double, 3> a{}, b{},
            point{{double(enemy.actor.position.x) * 32, double(enemy.actor.position.y) * 32,
                   -double(enemy.actor.position.z) * 32}};
        const double t0 = double(first - launched) / double(arrives - launched),
                     t1 = double(last - launched) / double(arrives - launched);
        for (size_t i = 0; i < 3; ++i) {
            const double delta = double(end[i]) - start[i];
            a[i] = start[i] + t0 * delta;
            b[i] = start[i] + t1 * delta;
        }
        // Remove the existing visual launch-height offset before body contact.
        a[2] += lift;
        b[2] += lift;
        double distance = 0;
        if (!SegmentDistanceSquared(point, a, b, distance))
            return -1;
        if (distance <= ProjectileContactRadius * ProjectileContactRadius) {
            change.agent = enemy.actor.agent;
            change.identity = enemy.actor.identity;
            change.before = enemy.actor.health;
            bool blinded = false;
            if (!Blindness::Active(build, c, blinded))
                return -1;
            if (blinded || enemy.evading || enemy.blocking) {
                change.after = change.before;
                change.missed = blinded;
                change.evaded = !blinded && enemy.evading;
                change.blocked = !blinded && !enemy.evading;
                return 1;
            }
            float damage = 0;
            if (!HitDamage(build, c.skillEffects, c.combatLevel, enemy.positiveEffects, enemy.level, c.wallNow, damage,
                           c.combatTrait) ||
                !combat::InfuseLight::HitHealth(build, enemy.positiveEffects, c.wallNow, health, damage, maximum,
                                                change.after))
                return -1;
            return 1;
        }
        sampled = true;
        sampledAt = last;
        if (c.now >= arrives)
            *this = {};
        return 0;
    }
};
} // namespace combat
} // namespace glide
