#pragma once
#include "glide/profile/dragon_facet_data.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace glide {
namespace modern {
// The official history adds self superspeed in2018: HoT grants allies only. Radius450 is a local selection policy;
// authored360 is launch distance, not radius. No hostile launch/damage or movement-speed modifier is simulated by
// this state-publication adapter.
enum class DragonReleaseStatus { None, Ready, Expired, Invalid };
struct DragonReleasePlan {
    std::vector<ReceivedWorldEffect> own;
    std::vector<WorldReceivedEffectTarget> allies;
};
template <unsigned Build> class DragonRelease {
    uint64_t at_ = 0, utc_ = 0;
    bool pending_ = false;

  public:
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported release dialect");
    enum : unsigned { Skill = 28075, Base = 27760, Buff = 5974, Radius = 450, MaximumAllies = Build == 49284 ? 5 : 4 };
    static unsigned Duration() {
        return Build == 49284 ? 3000u : 5000u;
    }
    static unsigned Activation(bool water) {
        return FacetData<Build>::RuleFor(Skill, water).activation;
    }
    static unsigned Animation(bool water) {
        return FacetData<Build>::RuleFor(Skill, water).animation;
    }
    static unsigned Recharge() {
        return FacetData<Build>::RuleFor(Skill).recharge;
    }
    bool Pending() const {
        return pending_;
    }
    bool Due(uint64_t now) const {
        return pending_ && now >= at_;
    }
    bool CancelIfPending(uint64_t impactUtc) {
        if (!impactUtc || !pending_ || utc_ <= impactUtc)
            return false;
        Cancel();
        return true;
    }
    void Cancel() {
        pending_ = false;
        at_ = utc_ = 0;
    }
    bool Schedule(const WorldActionContext& c, bool water = false) {
        const auto delay = Activation(water);
        if (pending_ || !c.wallNow || c.now > UINT64_MAX - delay - Duration() ||
            c.wallNow > UINT64_MAX - delay - Duration())
            return false;
        pending_ = true;
        at_ = c.now + delay;
        utc_ = c.wallNow + delay;
        return true;
    }
    DragonReleaseStatus Prepare(const WorldActionContext& c, const std::vector<WorldConditionActor>& allies,
                                DragonReleasePlan& out) const {
        if (!Due(c.now))
            return DragonReleaseStatus::None;
        if (!c.wallNow || c.wallNow < utc_ || !std::isfinite(c.position.x) || !std::isfinite(c.position.y) ||
            !std::isfinite(c.position.z))
            return DragonReleaseStatus::Invalid;
        const auto end = utc_ + Duration();
        if (c.wallNow >= end)
            return DragonReleaseStatus::Expired;
        DragonReleasePlan plan;
        const unsigned selfCount = Build == 49284 ? 0u : 1u;
        for (unsigned i = 0; i < selfCount; ++i) {
            ReceivedWorldEffect q;
            q.definition = Buff;
            q.endsUtc = end;
            plan.own.push_back(q);
        }
        std::vector<std::pair<double, const WorldConditionActor*>> candidates;
        std::set<unsigned> agents;
        std::set<std::string> identities;
        for (const auto& ally : allies) {
            if (!ally.agent || ally.agent == c.peerAgent || ally.identity.empty() || ally.identity == c.selfIdentity ||
                !agents.insert(ally.agent).second || !identities.insert(ally.identity).second)
                continue;
            const double x = (double(ally.position.x) - c.position.x) * 32,
                         y = (double(ally.position.y) - c.position.y) * 32,
                         z = (double(ally.position.z) - c.position.z) * 32;
            const double distance = x * x + y * y + z * z;
            if (std::isfinite(distance) && distance <= double(Radius) * Radius)
                candidates.push_back({distance, &ally});
        }
        std::sort(candidates.begin(), candidates.end(),
                  [](const std::pair<double, const WorldConditionActor*>& a,
                     const std::pair<double, const WorldConditionActor*>& b) {
                      return a.first != b.first ? a.first < b.first : a.second->identity < b.second->identity;
                  });
        if (candidates.size() > MaximumAllies)
            candidates.resize(MaximumAllies);
        for (const auto& candidate : candidates) {
            const auto& ally = *candidate.second;
            WorldReceivedEffectTarget q;
            q.agent = ally.agent;
            q.identity = ally.identity;
            q.definition = Buff;
            q.endsUtc = end;
            plan.allies.push_back(q);
        }
        out = std::move(plan);
        return DragonReleaseStatus::Ready;
    }
};
} // namespace modern
} // namespace glide
