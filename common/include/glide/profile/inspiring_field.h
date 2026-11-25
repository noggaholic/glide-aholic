#pragma once
#include "glide/profile/world_actions.h"
#include "glide/profile/segment_distance.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace glide {
namespace modern {
// Personal/allied1122 only; no enemy condition, damage, control prevention or visual. This connection-local field
// is deliberately not serialized: reconnect/leave cancels it.
enum class InspiringFieldStatus { None, Ready, Expired, Invalid };
struct InspiringFieldPlan {
    uint64_t revision = 0, nominalUtc = 0;
    unsigned pulseIndex = 0;
    std::string casterIdentity;
    bool own = false;
    ReceivedWorldEffect ownEffect;
    std::vector<WorldReceivedEffectTarget> allies;
};
template <unsigned Build> struct InspiringFieldRules {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported field dialect");
    enum : unsigned {
        Skill = 28516,
        Buff = 1122,
        Width = 180,
        Radius = 90,
        LifetimeMs = 5000,
        PulseMs = 1000,
        Pulses = 5,
        MaximumAllies = 4
    };
    static constexpr bool Circle = false;
    static unsigned MaximumRange() {
        return Build == 49284 ? 600u : 900u;
    }
    static unsigned GrantDurationMs() {
        return Build == 49284 ? 6000u : 3000u;
    }
};
template <class Rules> class AlliedField {
    struct Contribution {
        unsigned instance = 0;
        uint64_t endsUtc = 0;
    };
    struct Candidate {
        const WorldConditionActor* actor = nullptr;
        double distance = 0;
    };
    CatalogPosition origin_, destination_;
    uint64_t activationAt_ = 0, activationUtc_ = 0, revision_ = 0;
    unsigned nextPulse_ = 0;
    bool active_ = false;
    std::string casterIdentity_;
    std::map<std::string, Contribution> contributions_;
    static bool PositionValid(const CatalogPosition& p) {
        return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z);
    }
    unsigned Refresh(const std::string& identity, uint64_t utc) const {
        const auto at = contributions_.find(identity);
        return at != contributions_.end() && at->second.endsUtc > utc ? at->second.instance : 0;
    }

  public:
    enum : unsigned {
        Skill = Rules::Skill,
        Buff = Rules::Buff,
        Width = Rules::Width,
        Radius = Rules::Radius,
        LifetimeMs = Rules::LifetimeMs,
        PulseMs = Rules::PulseMs,
        Pulses = Rules::Pulses,
        MaximumAllies = Rules::MaximumAllies
    };
    static unsigned MaximumRange() {
        return Rules::MaximumRange();
    }
    static unsigned GrantDurationMs() {
        return Rules::GrantDurationMs();
    }
    bool Active() const {
        return active_;
    }
    unsigned NextPulse() const {
        return nextPulse_;
    }
    bool Begin(const CatalogPosition& origin, const CatalogPosition& destination, uint64_t activationAt,
               uint64_t activationUtc) {
        if (active_ || !activationUtc || !PositionValid(origin) || !PositionValid(destination) ||
            activationAt > UINT64_MAX - LifetimeMs || activationUtc > UINT64_MAX - (LifetimeMs + GrantDurationMs()) ||
            revision_ == UINT64_MAX)
            return false;
        const double x = (double(destination.x) - origin.x) * 32, y = (double(destination.y) - origin.y) * 32,
                     z = (double(destination.z) - origin.z) * 32;
        const double length = x * x + y * y + z * z;
        if (!std::isfinite(length) ||
            (Rules::Circle ? length != 0 : length <= 0 || length > double(MaximumRange()) * MaximumRange()))
            return false;
        origin_ = origin;
        destination_ = destination;
        activationAt_ = activationAt;
        activationUtc_ = activationUtc;
        nextPulse_ = 0;
        contributions_.clear();
        casterIdentity_.clear();
        active_ = true;
        ++revision_;
        return true;
    }
    // Squared native-unit distance to a closed3D segment, including its end caps.
    bool DistanceSquared(const CatalogPosition& point, double& out) const {
        if (!active_ || !PositionValid(point))
            return false;
        const std::array<double, 3> p{{double(point.x) * 32, double(point.y) * 32, double(point.z) * 32}},
            a{{double(origin_.x) * 32, double(origin_.y) * 32, double(origin_.z) * 32}},
            b{{double(destination_.x) * 32, double(destination_.y) * 32, double(destination_.z) * 32}};
        return SegmentDistanceSquared(p, a, Rules::Circle ? a : b, out);
    }
    // Allies have already been filtered for map/visibility/life/friendly policy.
    // No callback or state mutation: NotApplied simply discards the plan.
    InspiringFieldStatus Prepare(const WorldActionContext& context, const std::string& casterIdentity,
                                 const std::vector<WorldConditionActor>& allies, InspiringFieldPlan& out) const {
        if (!active_)
            return InspiringFieldStatus::None;
        if (!context.peerAgent || !context.wallNow || casterIdentity.empty() || !PositionValid(context.position) ||
            (!casterIdentity_.empty() && casterIdentity_ != casterIdentity) || revision_ == UINT64_MAX)
            return InspiringFieldStatus::Invalid;
        if (context.now < activationAt_)
            return InspiringFieldStatus::None;
        if (context.now - activationAt_ >= LifetimeMs)
            return InspiringFieldStatus::Expired;
        const auto pulse = static_cast<unsigned>((context.now - activationAt_) / PulseMs);
        if (pulse < nextPulse_)
            return InspiringFieldStatus::None;
        const uint64_t nominal = activationUtc_ + uint64_t(pulse) * PulseMs, ends = nominal + GrantDurationMs();
        if (context.wallNow < nominal)
            return InspiringFieldStatus::Invalid;
        if (context.wallNow >= ends)
            return InspiringFieldStatus::Expired;
        std::set<uint16_t> agents;
        std::set<std::string> identities;
        std::vector<Candidate> selected;
        for (const auto& actor : allies) {
            if (!actor.agent || actor.agent == context.peerAgent || actor.identity.empty() ||
                actor.identity == casterIdentity || !agents.insert(actor.agent).second ||
                !identities.insert(actor.identity).second)
                return InspiringFieldStatus::Invalid;
            double distance = 0;
            if (!DistanceSquared(actor.position, distance))
                return InspiringFieldStatus::Invalid;
            if (distance <= double(Radius) * Radius)
                selected.push_back({&actor, distance});
        }
        std::sort(selected.begin(), selected.end(), [](const Candidate& a, const Candidate& b) {
            return a.distance != b.distance ? a.distance < b.distance : a.actor->identity < b.actor->identity;
        });
        if (selected.size() > MaximumAllies)
            selected.resize(MaximumAllies);
        InspiringFieldPlan plan;
        plan.revision = revision_;
        plan.pulseIndex = pulse;
        plan.nominalUtc = nominal;
        plan.casterIdentity = casterIdentity;
        double ownDistance = 0;
        if (!DistanceSquared(context.position, ownDistance))
            return InspiringFieldStatus::Invalid;
        if (ownDistance <= double(Radius) * Radius) {
            plan.own = true;
            plan.ownEffect.definition = Buff;
            plan.ownEffect.refreshInstance = Refresh(casterIdentity, context.wallNow);
            plan.ownEffect.endsUtc = ends;
        }
        for (const auto& candidate : selected) {
            WorldReceivedEffectTarget target;
            target.agent = candidate.actor->agent;
            target.identity = candidate.actor->identity;
            target.definition = Buff;
            target.refreshInstance = Refresh(target.identity, context.wallNow);
            target.endsUtc = ends;
            plan.allies.push_back(std::move(target));
        }
        out = std::move(plan);
        return InspiringFieldStatus::Ready;
    }
    // Call only after the durable batch returned Applied. AllocatedIDs preserve
    // plan.allies order; ownInstance belongs to the separately prepared caster.
    // Invalid/stale results leave this pure state unchanged; caller must close
    // after an unexpected post-commit mismatch, never retry a committed batch.
    bool Commit(const InspiringFieldPlan& plan, const std::vector<unsigned>& allocated, unsigned ownInstance = 0) {
        if (!active_ || plan.revision != revision_ || revision_ == UINT64_MAX || plan.pulseIndex < nextPulse_ ||
            plan.pulseIndex >= Pulses || plan.nominalUtc != activationUtc_ + uint64_t(plan.pulseIndex) * PulseMs ||
            plan.casterIdentity.empty() || (!casterIdentity_.empty() && casterIdentity_ != plan.casterIdentity) ||
            plan.allies.size() > MaximumAllies || allocated.size() != plan.allies.size() ||
            (plan.own ? !ownInstance : ownInstance != 0))
            return false;
        const uint64_t ends = plan.nominalUtc + GrantDurationMs();
        if (plan.own && (plan.ownEffect.definition != Buff || plan.ownEffect.endsUtc != ends ||
                         (plan.ownEffect.refreshInstance && plan.ownEffect.refreshInstance != ownInstance)))
            return false;
        auto next = contributions_;
        std::set<std::string> identities;
        for (size_t i = 0; i < allocated.size(); ++i) {
            const auto& target = plan.allies[i];
            if (!allocated[i] || !target.agent || target.identity.empty() || target.identity == plan.casterIdentity ||
                !identities.insert(target.identity).second || target.definition != Buff || target.endsUtc != ends ||
                (target.refreshInstance && target.refreshInstance != allocated[i]))
                return false;
            next[target.identity] = {allocated[i], ends};
        }
        if (plan.own)
            next[plan.casterIdentity] = {ownInstance, ends};
        contributions_ = std::move(next);
        casterIdentity_ = plan.casterIdentity;
        nextPulse_ = plan.pulseIndex + 1;
        ++revision_;
        return true;
    }
    bool CancelIfPending(uint64_t impactUtc) {
        if (!impactUtc || !active_ || activationUtc_ <= impactUtc)
            return false;
        Cancel();
        return true;
    }
    void Cancel() {
        active_ = false;
        nextPulse_ = 0;
        contributions_.clear();
        casterIdentity_.clear();
        if (revision_ != UINT64_MAX)
            ++revision_;
    }
};
template <unsigned Build> using InspiringField = AlliedField<InspiringFieldRules<Build>>;
} // namespace modern
} // namespace glide
