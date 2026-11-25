#pragma once
#include "glide/profile/modern_conditions.h"
#include <cmath>
#include <string>
#include <vector>

namespace glide {
namespace modern {
// No damage or trait variants.
struct PainAbsorptionGrant {
    unsigned buff = 0;
    uint64_t startsUtc = 0, endsUtc = 0;
};
struct PainAbsorptionPlan {
    std::string ownConditions;
    std::vector<WorldConditionChange> donors;
    std::vector<PainAbsorptionGrant> grants; // Caster only, never one grant per donor.
    unsigned transferredKinds = 0;
};

template <unsigned Build> class PainAbsorption {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported absorption dialect");
    struct Candidate {
        const WorldConditionActor* actor = nullptr;
        double distance = 0;
    };
    static bool PositionValid(const CatalogPosition& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    static bool Grant(unsigned buff, unsigned duration, uint64_t activation, uint64_t utc, PainAbsorptionPlan& out) {
        if (activation > UINT64_MAX - duration)
            return false;
        if (activation + duration <= utc)
            return true;
        PainAbsorptionGrant grant;
        grant.buff = buff;
        grant.startsUtc = activation;
        grant.endsUtc = activation + duration;
        out.grants.push_back(grant);
        return true;
    }

  public:
    enum : unsigned {
        Skill = 27322,
        Slot = 1,
        MaximumTargets = 5,
        MaximumAllies = 4,
        Radius = 600,
        ExtraResistanceMs = 1000
    };
    static unsigned Legend() {
        return Build == 49284 ? 14u : Build == 167409 ? 17u : 18u;
    }
    static unsigned Cost() {
        return Build == 49284 ? 35u : 30u;
    }
    static unsigned ActivationMs(bool) {
        return Build == 49284 ? 1200u : 650u;
    }
    static unsigned AnimationMs(bool water) {
        return Build == 49284 ? 1800u : water ? 1833u : 1170u;
    }
    static unsigned RechargeMs() {
        return 0;
    }
    static unsigned BaseResistanceMs() {
        return Build == 49284 ? 2000u : 3000u;
    }
    static unsigned ResolutionMs() {
        return Build == 49284 ? 0u : 5000u;
    }
    static unsigned SelfCondition() {
        return Build == 49284 ? 720u : 0u;
    }
    static unsigned SelfConditionMs() {
        return Build == 49284 ? 8000u : 0u;
    }

    // Pure calculation. The server already filters allies by map, visibility,
    // life and its explicit friendly-player policy. Identity is opaque and must
    // be validated unchanged by the eventual multi-account transaction.
    static bool Prepare(uint16_t caster, const CatalogPosition& position, const std::string& ownConditions,
                        const std::vector<WorldConditionActor>& allies, uint64_t activationUtc, uint64_t nowUtc,
                        PainAbsorptionPlan& out, std::string& error) {
        error = "Invalid absorption snapshot or activation clock";
        if (!caster || !activationUtc || nowUtc < activationUtc || !PositionValid(position))
            return false;
        ConditionState<Build> own;
        if (!own.Load(ownConditions))
            return false;
        own.Expire(nowUtc);
        std::set<uint16_t> agents;
        std::set<std::string> identities;
        std::vector<Candidate> candidates;
        for (const auto& actor : allies) {
            if (!actor.agent || actor.agent == caster || actor.identity.empty() || !PositionValid(actor.position) ||
                !agents.insert(actor.agent).second || !identities.insert(actor.identity).second)
                return false;
            const double x = (double(actor.position.x) - position.x) * 32,
                         y = (double(actor.position.y) - position.y) * 32,
                         z = (double(actor.position.z) - position.z) * 32;
            const double distance = x * x + y * y + z * z;
            if (distance > double(Radius) * Radius)
                continue;
            Candidate candidate;
            candidate.actor = &actor;
            candidate.distance = distance;
            candidates.push_back(candidate);
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            if (a.distance != b.distance)
                return a.distance < b.distance;
            return a.actor->identity < b.actor->identity;
        });
        if (candidates.size() > MaximumAllies)
            candidates.resize(MaximumAllies);
        PainAbsorptionPlan candidate;
        for (const auto& selected : candidates) {
            const auto& actor = *selected.actor;
            ConditionState<Build> donor;
            error = "Invalid allied conditions or absorption capacity exceeded";
            if (!donor.Load(actor.conditions))
                return false;
            unsigned kind = 0;
            std::vector<unsigned> instances;
            // Same oldest-applied-kind order as ConditionState::RemoveKinds.
            for (const auto& entry : donor.Entries())
                if (entry.startsUtc <= nowUtc && nowUtc < entry.endsUtc) {
                    if (!kind)
                        kind = entry.definition;
                    if (entry.definition == kind)
                        instances.push_back(entry.instance);
                }
            if (instances.empty())
                continue;
            // TransferTo assigns destination instances and startsUtc=nowUtc,
            // preserving each endsUtc. It does not refresh remaining duration.
            if (!donor.TransferTo(own, instances, nowUtc))
                return false;
            WorldConditionChange change;
            change.agent = actor.agent;
            change.identity = actor.identity;
            change.before = actor.conditions;
            change.after = donor.Text();
            candidate.donors.push_back(change);
            ++candidate.transferredKinds;
        }
        const unsigned self = SelfCondition(), selfDuration = SelfConditionMs();
        if (self) {
            if (activationUtc > UINT64_MAX - selfDuration)
                return false;
            if (nowUtc - activationUtc < selfDuration && !own.Add(self, activationUtc, selfDuration))
                return false;
        }
        if (!Grant(26980, BaseResistanceMs() + ExtraResistanceMs * candidate.transferredKinds, activationUtc, nowUtc,
                   candidate))
            return false;
        const unsigned resolution = ResolutionMs();
        if (resolution && !Grant(873, resolution, activationUtc, nowUtc, candidate))
            return false;
        candidate.ownConditions = own.Text();
        out = std::move(candidate);
        error.clear();
        return true;
    }
};
} // namespace modern
} // namespace glide
