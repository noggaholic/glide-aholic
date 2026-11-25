#pragma once
#include "glide/profile/pain_absorption.h"

namespace glide {
namespace hot49284 {
// Pending cast only. The StarterActions candidate owns this object and commits
// it only after the atomic transfer result is Applied. A failed write retries
// the same activation and does not charge energy or allocate instances again.
class PainAbsorptionAction {
    uint64_t due_ = 0, activationUtc_ = 0;

  public:
    using Rules = modern::PainAbsorption<49284>;
    bool Pending() const {
        return due_ != 0;
    }
    bool Due(uint64_t now) const {
        return due_ && now >= due_;
    }
    uint64_t ActivationUtc() const {
        return activationUtc_;
    }
    void Cancel() {
        due_ = activationUtc_ = 0;
    }
    bool Schedule(const WorldActionContext& context, bool water) {
        const auto delay = Rules::ActivationMs(water);
        if (Pending() || !context.wallNow || context.now > UINT64_MAX - delay || context.wallNow > UINT64_MAX - delay)
            return false;
        due_ = context.now + delay;
        activationUtc_ = context.wallNow + delay;
        return true;
    }
    bool Prepare(const WorldActionContext& context, modern::PainAbsorptionPlan& out, std::string& error) const {
        if (!Due(context.now) || !context.conditionAllies) {
            error = "Absorption activation or allied snapshot unavailable";
            return false;
        }
        std::vector<WorldConditionActor> allies;
        if (!context.conditionAllies(allies)) {
            error = "Allied absorption snapshot failed";
            return false;
        }
        return Rules::Prepare(context.peerAgent, context.position, context.conditionState, allies, activationUtc_,
                              context.wallNow, out, error);
    }
    static WorldConditionCommit Persist(const WorldActionContext& context, const std::string& deadlines,
                                        const std::string& effects, const std::string& health,
                                        const modern::PainAbsorptionPlan& plan) {
        return context.persistConditionTransfer
                   ? context.persistConditionTransfer(deadlines, effects, health, plan.ownConditions, plan.donors)
                   : WorldConditionCommit::NotApplied;
    }
    // RecoveryRequired must be propagated as Failed/connection termination.
    // It is never safe to treat a possibly committed transaction as a retry.
};
} // namespace hot49284
} // namespace glide
