#include "glide/profile/demon_leap_impact.h"
#include "glide/profile/elemental_blast.h"
#include "glide/profile/burst_strength_impact.h"
#include "glide/profile/blindness.h"
#include "glide/profile/infuse_light.h"
#pragma once
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_water_healing.h"
#include "glide/profile/beta_combat_defense.h"
#include "glide/profile/combat_damage.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/jade_winds.h"
#include "glide/profile/forced_engagement.h"
#include "glide/profile/basic_attack_contacts.h"
#include <cmath>
#include <algorithm>
namespace glide {
namespace beta {
// First compatible damage policy, not recovered weapon/armor calculations.
inline bool MeleeCombatSkill(unsigned build, unsigned skill) {

    if (build == 14075 || build == 14422 || build == 14505 || build == 14812) {
        switch (skill) {
        case 115:
        case 268:
        case 901:
        case 902:
        case 965:
        case 2506:
        case 2507:
        case 2508:
        case 2523:
        case 2524:
        case 2525:
        case 4282:
        case 6078:
        case 7799:
        case 8959:
        case 8960:
        case 8961:
        case 11305:
        case 11306:
            return true;
        case 2554:
        case 2555:
        case 2556:
            return build == 14075;
        default:
            return false;
        }
    }
    if (build != 49284 && build != 167409 && build != 204489)
        return false;
    if (skill == 27917 || skill == 27162 || skill == 28113 || skill == 27505 || skill == 28406 || skill == 26679)
        return true;
    switch (skill) {
    case 9108:
    case 9109:
    case 9110:
    case 10170:
    case 10171:
    case 10172:
    case 10617:
    case 10692:
    case 10693:
    case 13004:
    case 13087:
    case 13108:
    case 13119:
    case 13120:
    case 13121:
    case 14363:
    case 14364:
    case 14365:
    case 14437:
    case 14438:
    case 14439:
    case 26666:
    case 26730:
    case 27066:
        return true;
    // Remaining authored basics use the same compatible directed contact.
    // Registered projectile consumers are excluded by this own-data registry.
    default:
        return combat::BasicContactSkill(build, skill);
    }
}
inline float CombatMaximumHealth(unsigned build) {
    return BetaHealthBuild(build) ? BetaHealthMaximum(build)
           : build == 49284       ? modern::kHotMaximumHealth
                                  : modern::kPublishedMaximumHealth;
}
inline uint16_t CombatLocalActor(unsigned build, uint16_t actor) {
    return build == 167409 || build == 204489 ? 57344 : actor;
}
inline bool CombatHealthUpdate(unsigned build, uint16_t actor, unsigned skill, float before, float after, Buffer& out,
                               uint16_t source = 0) {
    if (BetaHealthBuild(build))
        return BetaHealthUpdate(build, actor, after, out);
    return build == 49284    ? modern::HealthDelta<49284>(actor, skill, after - before, source, out)
           : build == 167409 ? modern::HealthDelta<167409>(actor, skill, after - before, source, out)
           : build == 204489 ? modern::HealthDelta<204489>(actor, skill, after - before, source, out)
                             : false;
}
inline float MeleeCombatDamage() {
    return 1.0f;
}
// Own healing parameters, extra Healing attribute zero. Healing on a valid
// third strike remains an explicit compatibility decision.
inline float MeleeCombatHealing(unsigned skill, unsigned level) {
    if (!level || level > 80)
        return 0;
    return skill == 2523 ? 15.0f + 0.07f * level * level : skill == 4282 ? 5.0f + 0.01f * level * level : 0.0f;
}
inline float MeleeCombatHealing(unsigned build, unsigned skill, unsigned level) {
    if (BetaHealthBuild(build))
        return MeleeCombatHealing(skill, level);
    if ((build != 49284 && build != 167409 && build != 204489) || !level || level > 80)
        return 0;
    // Zero extra Healing attribute; execution on the valid third strike is local policy.
    return skill == 9108    ? 15.0f + (build == 49284 ? 0.05f : 0.055f) * level * level
           : skill == 10617 ? 5.0f + 0.01f * level * level
                            : 0.0f;
}
inline bool MeleeAreaHealing(unsigned build, unsigned skill) {
    return BetaHealthBuild(build) ? skill == 2523
                                  : (build == 49284 || build == 167409 || build == 204489) && skill == 9108;
}
constexpr float kMeleeHealingRadius = 150.0f;
constexpr size_t kMeleeHealingLimit = 5;
constexpr size_t kCounterTargetLimit = 5;
inline bool CounterAreaContains(const WorldActionContext& c, const CatalogPosition& p, double& distance) {
    const double dx = (double(p.x) - c.position.x) * 32, dy = (double(p.y) - c.position.y) * 32,
                 dz = (double(p.z) - c.position.z) * 32;
    distance = std::hypot(std::hypot(dx, dy), dz);
    // Half-circle facing the current aim. Missing aim uses the existing +X default.
    const double fx = c.hasFacing ? c.facing.x : 1, fy = c.hasFacing ? c.facing.y : 0;
    const double length = std::hypot(fx, fy);
    return std::isfinite(distance) && distance <= 170 && std::isfinite(length) && length > 0 && dx * fx + dy * fy >= 0;
}
inline int SettleCounterArea(unsigned build, const WorldActionContext& c, WorldActionOutput& out,
                             WorldActionOutput* prefix) {
    WorldActionOutput prepared;
    prepared.sequences = 0;
    if (prefix)
        prepared = std::move(*prefix);
    std::vector<WorldCombatTarget> enemies;
    if (!c.combatEnemies || !c.combatEnemies(enemies))
        return -1;
    struct Candidate {
        double distance;
        WorldCombatTarget enemy;
    };
    std::vector<Candidate> candidates;
    const float maximum = CombatMaximumHealth(build);
    float own = 0;
    if (!modern::ReadHealthState(c.healthState, own, maximum))
        return -1;
    if (own <= 0) {
        out = std::move(prepared);
        return 2;
    }
    for (auto& enemy : enemies) {
        double distance = 0;
        float health = 0;
        if (enemy.actor.agent && enemy.actor.agent != c.peerAgent && !enemy.actor.identity.empty() &&
            CounterAreaContains(c, enemy.actor.position, distance) &&
            modern::ReadHealthState(enemy.actor.health, health, maximum) && health > 0)
            candidates.push_back({distance, std::move(enemy)});
    }
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.distance != b.distance ? a.distance < b.distance : a.enemy.actor.agent < b.enemy.actor.agent;
    });
    bool blinded = false;
    if (!combat::Blindness::Active(build, c, blinded))
        return -1;
    std::vector<WorldHealthChange> changes;
    for (const auto& candidate : candidates) {
        const auto& e = candidate.enemy;
        bool duplicate = false;
        for (const auto& old : changes)
            if (old.agent == e.actor.agent)
                duplicate = true;
        if (duplicate)
            continue;
        WorldHealthChange hit;
        hit.agent = e.actor.agent;
        hit.identity = e.actor.identity;
        hit.before = e.actor.health;
        hit.missed = blinded;
        hit.evaded = !blinded && e.evading;
        hit.blocked = !blinded && !e.evading && e.blocking;
        if (hit.missed || hit.blocked || hit.evaded)
            hit.after = hit.before;
        else {
            float before = 0, damage = 0;
            if (!modern::ReadHealthState(hit.before, before, maximum) ||
                !combat::HitDamage(build, c.skillEffects, c.combatLevel, e.positiveEffects, e.level, c.wallNow, damage,
                                   c.combatTrait) ||
                !combat::InfuseLight::HitHealth(build, e.positiveEffects, c.wallNow, before, damage, maximum,
                                                hit.after))
                return -1;
        }
        changes.push_back(std::move(hit));
        if (changes.size() == kCounterTargetLimit)
            break;
    }
    if (changes.empty()) {
        out = std::move(prepared);
        return 2;
    }
    if (!c.persistCombatHits || !c.ReserveSequences(prepared.sequences))
        return -1;
    const auto saved = c.persistCombatHits(965, c.healthState, changes);
    if (saved == WorldConditionCommit::RecoveryRequired)
        return -1;
    if (saved != WorldConditionCommit::Applied)
        return 0;
    out = std::move(prepared);
    return 2;
}
// Explicit piercing-line adaptation: existing contact radius, own range and five-target cap.
inline int SettleBanishArea(unsigned build, uint32_t selected, const std::string& identity, uint64_t nominal,
                            const WorldActionContext& c, WorldActionOutput& out) {
    out = {};
    out.sequences = 0;
    WorldCombatTarget aim;
    float sourceHealth = 0, aimHealth = 0;
    const auto maximum = CombatMaximumHealth(build);
    if (!modern::ReadHealthState(c.healthState, sourceHealth, maximum))
        return -1;
    if (sourceHealth <= 0 || !c.combatTarget || !c.combatTarget(selected, aim) || aim.actor.identity != identity ||
        !modern::ReadHealthState(aim.actor.health, aimHealth, maximum) || aimHealth <= 0 ||
        !combat::BanishEnchantment::Contains(build, c.position, aim.actor.position, aim.actor.position))
        return 2;
    std::vector<WorldCombatTarget> enemies;
    if (!c.combatEnemies || !c.combatEnemies(enemies))
        return -1;
    std::sort(enemies.begin(), enemies.end(), [&](const WorldCombatTarget& a, const WorldCombatTarget& b) {
        if (a.actor.agent == selected || b.actor.agent == selected)
            return a.actor.agent == selected && b.actor.agent != selected;
        const auto distance = [&](const CatalogPosition& p) {
            return std::hypot(std::hypot(double(p.x) - c.position.x, double(p.y) - c.position.y),
                              double(p.z) - c.position.z);
        };
        const double da = distance(a.actor.position), db = distance(b.actor.position);
        return da != db ? da < db : a.actor.agent < b.actor.agent;
    });
    bool blinded = false;
    if (!combat::Blindness::Active(build, c, blinded))
        return -1;
    std::vector<WorldHealthChange> changes;
    std::set<unsigned> used;
    for (const auto& enemy : enemies) {
        const auto& e = enemy.actor;
        float health = 0;
        if (!e.agent || e.agent == c.peerAgent || e.identity.empty() || !used.insert(e.agent).second ||
            !combat::BanishEnchantment::Contains(build, c.position, aim.actor.position, e.position) ||
            !modern::ReadHealthState(e.health, health, maximum) || health <= 0)
            continue;
        WorldHealthChange hit;
        hit.agent = e.agent;
        hit.identity = e.identity;
        hit.before = e.health;
        hit.impactUtc = nominal;
        hit.missed = blinded;
        hit.evaded = !blinded && enemy.evading;
        if (hit.missed || hit.evaded)
            hit.after = hit.before;
        else {
            float damage = 0;
            if (!combat::HitDamage(build, c.skillEffects, c.combatLevel, enemy.positiveEffects, enemy.level, c.wallNow,
                                   damage, c.combatTrait) ||
                !combat::InfuseLight::HitHealth(build, enemy.positiveEffects, c.wallNow, health, damage, maximum,
                                                hit.after))
                return -1;
        }
        changes.push_back(std::move(hit));
        if (changes.size() == 5)
            break;
    }
    if (changes.empty() || changes.front().agent != selected)
        return 2;
    if (!c.persistCombatHits || !c.ReserveSequences(0))
        return -1;
    const auto saved = c.persistCombatHits(27505, c.healthState, changes);
    return saved == WorldConditionCommit::RecoveryRequired ? -1 : saved == WorldConditionCommit::Applied ? 2 : 0;
}
template <class Policy>
inline int SettleRadialArea(unsigned build, uint64_t nominal, const WorldActionContext& c, WorldActionOutput& out) {
    out = {};
    out.sequences = 0;
    float own = 0;
    const auto maximum = CombatMaximumHealth(build);
    if (!Policy::Build(build) || !modern::ReadHealthState(c.healthState, own, maximum))
        return -1;
    if (own <= 0)
        return 2;
    std::vector<WorldCombatTarget> enemies;
    if (!c.combatEnemies || !c.combatEnemies(enemies))
        return -1;
    const auto distance = [&](const CatalogPosition& p) {
        return std::hypot(std::hypot(double(p.x) - c.position.x, double(p.y) - c.position.y),
                          double(p.z) - c.position.z);
    };
    std::sort(enemies.begin(), enemies.end(), [&](const WorldCombatTarget& a, const WorldCombatTarget& b) {
        const auto da = distance(a.actor.position), db = distance(b.actor.position);
        return da != db ? da < db : a.actor.agent < b.actor.agent;
    });
    bool blinded = false;
    if (!combat::Blindness::Active(build, c, blinded))
        return -1;
    std::vector<WorldHealthChange> changes;
    std::set<unsigned> used;
    for (const auto& enemy : enemies) {
        const auto& e = enemy.actor;
        float health = 0;
        if (!e.agent || e.agent == c.peerAgent || e.identity.empty() || !used.insert(e.agent).second ||
            !Policy::Contains(c.position, e.position) || !modern::ReadHealthState(e.health, health, maximum) ||
            health <= 0)
            continue;
        WorldHealthChange hit;
        hit.agent = e.agent;
        hit.identity = e.identity;
        hit.before = e.health;
        hit.impactUtc = nominal;
        hit.missed = blinded;
        hit.evaded = !blinded && enemy.evading;
        hit.blocked = !blinded && !hit.evaded && enemy.blocking;
        if (hit.missed || hit.evaded || hit.blocked)
            hit.after = hit.before;
        else {
            float damage = 0;
            if (!combat::HitDamage(build, c.skillEffects, c.combatLevel, enemy.positiveEffects, enemy.level, c.wallNow,
                                   damage, c.combatTrait) ||
                !combat::InfuseLight::HitHealth(build, enemy.positiveEffects, c.wallNow, health, damage, maximum,
                                                hit.after))
                return -1;
        }
        changes.push_back(std::move(hit));
        if (changes.size() == Policy::Targets)
            break;
    }
    if (changes.empty())
        return 2;
    if (!c.persistCombatHits || !c.ReserveSequences(0))
        return -1;
    const auto saved = c.persistCombatHits(Policy::Skill, c.healthState, changes);
    return saved == WorldConditionCommit::RecoveryRequired ? -1 : saved == WorldConditionCommit::Applied ? 2 : 0;
}
inline int SettleJadeArea(unsigned build, uint64_t nominal, const WorldActionContext& c, WorldActionOutput& out) {
    return SettleRadialArea<combat::JadeWinds>(build, nominal, c, out);
}
inline int SettleElementalArea(unsigned build, uint64_t nominal, const WorldActionContext& c, WorldActionOutput& out) {
    return SettleRadialArea<combat::ElementalBlast>(build, nominal, c, out);
}
inline int SettleBurstArea(unsigned build, uint64_t nominal, const WorldActionContext& c, WorldActionOutput& out) {
    return SettleRadialArea<combat::BurstStrengthImpact>(build, nominal, c, out);
}
inline int SettleDemonLeapArea(unsigned build, uint64_t nominal, const WorldActionContext& c, WorldActionOutput& out) {
    return SettleRadialArea<combat::DemonLeapImpact>(build, nominal, c, out);
}
inline float MeleeHealingRadius(unsigned build) {
    return build == 167409 || build == 204489 ? 180.0f : kMeleeHealingRadius;
}
struct MeleeContact {
    unsigned skill = 0, identity = 0;
    uint32_t target = 0;
    std::string targetIdentity;
    float minimum = 0, maximum = 0;
    uint8_t medium = 0;
    uint64_t settlesAt = 0, impactUtc = 0;
    // Call on a disposable controller copy only.2 settled,1 idle,0 retry,-1 fatal.
    int Settle(unsigned build, uint16_t owner, unsigned level, const WorldActionContext& c, WorldActionOutput& out,
               WorldActionOutput* prefix = nullptr) {
        if (!skill)
            return 1;
        out = {};
        out.sequences = 0;
        if (skill == 27917) {
            const int result = SettleDemonLeapArea(build, impactUtc, c, out);
            if (result == 2)
                *this = {};
            return result;
        }
        if (skill == 27162) {
            const int result = SettleElementalArea(build, impactUtc, c, out);
            if (result == 2)
                *this = {};
            return result;
        }
        if (skill == 28113) {
            const int result = SettleBurstArea(build, impactUtc, c, out);
            if (result == 2)
                *this = {};
            return result;
        }
        if (skill == 28406) {
            const int result = SettleJadeArea(build, impactUtc, c, out);
            if (result == 2)
                *this = {};
            return result;
        }
        if (skill == 27505) {
            const int result = SettleBanishArea(build, target, targetIdentity, impactUtc, c, out);
            if (result == 2)
                *this = {};
            return result;
        }
        if (skill == 965 && BetaHealthBuild(build)) {
            const int result = SettleCounterArea(build, c, out, prefix);
            if (result == 2)
                *this = {};
            return result;
        }
        WorldActionOutput prepared;
        prepared.sequences = 0;
        if (prefix)
            prepared = std::move(*prefix);
        WorldCombatTarget enemy;
        if (!c.combatTarget || !c.combatTarget(target, enemy) || enemy.actor.agent != target || target == owner ||
            enemy.actor.identity != targetIdentity) {
            out = std::move(prepared);
            *this = {};
            return 2;
        }
        const double dx = (double(enemy.actor.position.x) - c.position.x) * 32,
                     dy = (double(enemy.actor.position.y) - c.position.y) * 32,
                     dz = (double(enemy.actor.position.z) - c.position.z) * 32;
        const double distance = std::hypot(std::hypot(dx, dy), dz);
        if (!std::isfinite(distance) || distance < minimum || distance > maximum) {
            out = std::move(prepared);
            *this = {};
            return 2;
        }
        float own = 0, before = 0;
        const float maximumHealth = CombatMaximumHealth(build);
        if (!modern::ReadHealthState(c.healthState, own, maximumHealth) ||
            !modern::ReadHealthState(enemy.actor.health, before, maximumHealth))
            return -1;
        if (own <= 0 || before <= 0) {
            out = std::move(prepared);
            *this = {};
            return 2;
        }
        WorldHealthChange change;
        change.impactUtc = impactUtc;
        change.agent = enemy.actor.agent;
        change.identity = enemy.actor.identity;
        change.before = enemy.actor.health;
        bool blinded = false;
        if (!combat::Blindness::Active(build, c, blinded))
            return -1;
        if (blinded || enemy.evading || enemy.blocking) {
            change.after = change.before;
            change.missed = blinded;
            change.evaded = !blinded && enemy.evading;
            change.blocked = !blinded && !enemy.evading;
            if (!c.persistCombatHit || !c.ReserveSequences(prepared.sequences))
                return -1;
            const auto saved = c.persistCombatHit(skill, c.healthState, change);
            if (saved == WorldConditionCommit::RecoveryRequired)
                return -1;
            if (saved != WorldConditionCommit::Applied)
                return 0;
            out = std::move(prepared);
            *this = {};
            return 2;
        }
        std::string ownAfter;
        std::vector<WorldHealthChange> allies;
        if (MeleeAreaHealing(build, skill)) {
            PreparedWaterHealing healing;
            const std::array<float, 3> point{{c.position.x * 32, c.position.y * 32, -c.position.z * 32}};
            if (!PrepareAreaHealing(build, owner, point, c, MeleeCombatHealing(build, skill, level),
                                    MeleeHealingRadius(build), kMeleeHealingLimit, healing, true, skill) ||
                !prepared.owned.Append(healing.owned.View()) || !prepared.peers.Append(healing.peers.View()))
                return -1;
            ownAfter = std::move(healing.ownHealth);
            allies = std::move(healing.targets);
        } else {
            if (!modern::WriteHealthState((std::min)(maximumHealth, own + MeleeCombatHealing(build, skill, level)),
                                          ownAfter, maximumHealth))
                return -1;
            float ownCurrent = 0;
            if (!modern::ReadHealthState(ownAfter, ownCurrent, maximumHealth))
                return -1;
            if (ownCurrent != own) {
                if (!CombatHealthUpdate(build, CombatLocalActor(build, owner), skill, own, ownCurrent,
                                        prepared.owned) ||
                    !CombatHealthUpdate(build, owner, skill, own, ownCurrent, prepared.peers))
                    return -1;
            }
        }
        float damage = 0;
        if (!combat::HitDamage(build, c.skillEffects, c.combatLevel ? c.combatLevel : level, enemy.positiveEffects,
                               enemy.level, c.wallNow, damage, c.combatTrait))
            return -1;
        if (!combat::InfuseLight::HitHealth(build, enemy.positiveEffects, c.wallNow, before, damage, maximumHealth,
                                            change.after) ||
            (allies.empty() ? !c.persistCombatHit : !c.persistCombatHitWithAllies) ||
            !c.ReserveSequences(prepared.sequences))
            return -1;
        const auto saved = allies.empty() ? c.persistCombatHit(skill, ownAfter, change)
                                          : c.persistCombatHitWithAllies(skill, ownAfter, change, allies);
        if (saved == WorldConditionCommit::RecoveryRequired)
            return -1;
        if (saved != WorldConditionCommit::Applied)
            return 0;
        out = std::move(prepared);
        *this = {};
        return 2;
    }
};
} // namespace beta
} // namespace glide
