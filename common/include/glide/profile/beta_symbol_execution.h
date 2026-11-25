#pragma once
#include "glide/profile/beta_positive_effects.h"
#include "glide/profile/beta_protection_data.h"

namespace glide {
namespace beta {
template <class Rules> class AlliedEffectExecution {
    modern::AlliedField<Rules> field_;
    uint64_t due_ = 0, utc_ = 0;

  public:
    bool Schedule(const WorldActionContext& c) {
        // Older standalone animation checks have no durable allied-effect host.
        // The server always supplies this capability.
        if (!c.persistPreparedAlliedEffects)
            return true;
        if (c.selfIdentity.empty() || !c.wallNow || c.now > UINT64_MAX - Rules::ActivationMs ||
            c.wallNow > UINT64_MAX - Rules::ActivationMs)
            return false;
        field_.Cancel();
        due_ = c.now + Rules::ActivationMs;
        utc_ = c.wallNow + Rules::ActivationMs;
        return true;
    }
    void CancelPending() {
        due_ = utc_ = 0;
    }
    void Cancel() {
        CancelPending();
        field_.Cancel();
    }
    // Return2 for an applied durable pulse,1 for no publication,0 for retry,-1 for failure.
    template <class Effects>
    int Advance(unsigned build, uint16_t actor, unsigned level, Effects& effects, uint64_t& version, bool& recovery,
                const std::string& deadlines, const WorldActionContext& c, WorldActionOutput& out,
                bool reserved = false) {
        if (due_ && c.now >= due_) {
            if (!field_.Begin(c.position, c.position, due_, utc_))
                return -1;
            CancelPending();
        }
        if (!field_.Active())
            return 1;
        std::vector<WorldConditionActor> allies;
        if (c.conditionAllies && !c.conditionAllies(allies))
            return -1;
        modern::InspiringFieldPlan plan;
        const auto status = field_.Prepare(c, c.selfIdentity, allies, plan);
        if (status == modern::InspiringFieldStatus::Invalid)
            return -1;
        if (status == modern::InspiringFieldStatus::Expired) {
            field_.Cancel();
            return 1;
        }
        if (status != modern::InspiringFieldStatus::Ready)
            return 1;
        if (!plan.own && plan.allies.empty())
            return field_.Commit(plan, {}, 0) ? 1 : -1;
        if (!Rules::Build(build) || version == UINT64_MAX || !c.persistPreparedAlliedEffects)
            return -1;
        auto candidate = effects;
        auto nextField = field_;
        WorldActionOutput prepared;
        unsigned ownInstance = 0;
        prepared.sequences = out.sequences;
        prepared.targetPeerAgent = out.targetPeerAgent;
        prepared.flight = out.flight;
        prepared.peerEffectVersion = out.peerEffectVersion;
        prepared.relocate = out.relocate;
        prepared.destination = out.destination;
        if (!prepared.owned.Append(out.owned.View()) || !prepared.peers.Append(out.peers.View()) ||
            !prepared.targetView.Append(out.targetView.View()))
            return -1;
        if (plan.own) {
            plan.ownEffect.sourceLevel = level;
            Buffer bytes;
            if (!candidate.Receive(actor, level, c, plan.ownEffect, ownInstance, bytes) ||
                !prepared.owned.Append(bytes.View()) || !prepared.peers.Append(bytes.View()))
                return -1;
        }
        if (!reserved && !c.ReserveSequences(out.sequences))
            return -1;
        std::vector<unsigned> allocated;
        const auto result = c.persistPreparedAlliedEffects(
            deadlines,
            [&](const std::vector<unsigned>& ids, std::string& text) {
                if (!nextField.Commit(plan, ids, ownInstance))
                    return false;
                text = candidate.Text();
                return !text.empty() && text.size() <= 8192;
            },
            plan.allies, allocated);
        if (result == WorldConditionCommit::RecoveryRequired) {
            recovery = true;
            return -1;
        }
        if (result != WorldConditionCommit::Applied)
            return 0;
        effects = std::move(candidate);
        field_ = std::move(nextField);
        if (plan.own)
            prepared.peerEffectVersion = ++version;
        out = std::move(prepared);
        return 2;
    }
};
using Symbol2527Execution = AlliedEffectExecution<Symbol2527>;
using Protection965Execution = AlliedEffectExecution<Protection965>;
} // namespace beta
} // namespace glide
