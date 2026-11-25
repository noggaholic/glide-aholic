#pragma once
#include "glide/profile/world_actions.h"
#include <algorithm>
#include <cmath>
#include <set>

namespace glide {
namespace modern {
// 27975 facts: personal26596/5000ms, described count5. Radius600 is an
// explicit local ally-selection policy, not a recovered geometry parameter.
// The caller supplies visible living same-map allies and its existing cast
// deadline. No additional agenda, mitigation, trait737 or combat is inferred.
enum class GreatDwarfAlliesStatus { None, Ready, Expired, Invalid };
struct GreatDwarfAlliesPlan {
    ReceivedWorldEffect own;
    std::vector<WorldReceivedEffectTarget> allies;
};
template <unsigned Build> struct GreatDwarfAllies {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported Rite dialect");
    enum : unsigned { Skill = 27975, Buff = 26596, Duration = 5000, Radius = 600, MaximumAllies = 4 };
    static unsigned Activation() {
        return Build == 49284 ? 3000u : 1667u;
    }
    static unsigned Animation() {
        return Build == 49284 ? 3500u : 2000u;
    }
    static GreatDwarfAlliesStatus Prepare(const WorldActionContext& c, uint64_t activationUtc,
                                          const std::vector<WorldConditionActor>& allies, GreatDwarfAlliesPlan& out) {
        if (!activationUtc)
            return GreatDwarfAlliesStatus::None;
        if (!c.wallNow || activationUtc > UINT64_MAX - Duration || !std::isfinite(c.position.x) ||
            !std::isfinite(c.position.y) || !std::isfinite(c.position.z))
            return GreatDwarfAlliesStatus::Invalid;
        if (c.wallNow < activationUtc)
            return GreatDwarfAlliesStatus::None;
        const auto ends = activationUtc + Duration;
        if (c.wallNow >= ends)
            return GreatDwarfAlliesStatus::Expired;
        GreatDwarfAlliesPlan plan;
        plan.own.definition = Buff;
        plan.own.endsUtc = ends;
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
        for (const auto& item : candidates) {
            WorldReceivedEffectTarget request;
            request.agent = item.second->agent;
            request.identity = item.second->identity;
            request.definition = Buff;
            request.endsUtc = ends;
            plan.allies.push_back(request);
        }
        out = std::move(plan);
        return GreatDwarfAlliesStatus::Ready;
    }
};
} // namespace modern
} // namespace glide
