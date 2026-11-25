#include "world_actions.h"
#include "motion.h"
#include "motion_state.h"
#include "glider.h"
#include "starter_actions.h"
#include <utility>
namespace glide {
namespace hot49284 {
namespace {
class HotActions final : public WeaponActions {
  public:
    explicit HotActions(uint16_t agent, std::unique_ptr<StarterActions> skills = {})
        : agent_(agent), skills_(std::move(skills)) {}
    bool EvadesIncoming(uint64_t now) const override {
        return skills_ && skills_->EvadesIncoming(now);
    }
    void SetEquipment(uint8_t equipped) override {
        if (skills_)
            skills_->SetEquipment(equipped);
    }
    WorldActionResult BasicAttackRequest(const WorldActionContext& c, uint16_t target, uint32_t token,
                                         Buffer& out) const override {
        return skills_ ? skills_->BasicAttackRequest(c, target, token, out) : WorldActionResult::Ignored;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        Reader request(input);
        uint16_t id = 0;
        if (request.U16(id) && id == 109) {
            if (!skills_)
                return WorldActionResult::Ignored;
            if (version_ == UINT64_MAX)
                return WorldActionResult::Failed;
            const auto result = skills_->Request(input, context, out);
            if (result == WorldActionResult::Handled && skills_->PeerEffectVersion() != skillsVersion_) {
                skillsVersion_ = skills_->PeerEffectVersion();
                ++version_;
                if (out.peers.Size())
                    out.peerEffectVersion = version_;
            }
            return result;
        }
        MotionMessage motion{};
        size_t consumed = 0;
        const auto status = DecodeMotion(input, motion, consumed);
        if (status == DecodeStatus::Unknown)
            return WorldActionResult::Ignored;
        if (status != DecodeStatus::Complete || consumed != input.size || context.peerAgent != agent_ ||
            !context.sequence || context.sequence == UINT32_MAX || version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        bool skillChanged = false;
        if (skills_) {
            const auto skill = skills_->Request(input, context, result);
            if (skill != WorldActionResult::Ignored) {
                if (skill == WorldActionResult::Failed)
                    return skill;
                if (skills_->PeerEffectVersion() != skillsVersion_) {
                    skillsVersion_ = skills_->PeerEffectVersion();
                    ++version_;
                    skillChanged = true;
                }
                if (motion.id != 16) {
                    if (skillChanged && result.peers.Size())
                        result.peerEffectVersion = version_;
                    out = std::move(result);
                    return skill;
                }
            }
        }
        result.sequences = 2;
        Buffer relay;
        const auto response = OwnMotion(input, agent_, context.sequence, relay);
        if (response == RelayResult::Invalid)
            return WorldActionResult::Failed;
        if (relay.Size() && (!result.owned.Append(relay.View()) || !result.peers.Append(relay.View())))
            return WorldActionResult::Failed;
        auto next = glider_;
        Buffer effect;
        const auto transition = next.Update(input, agent_, context.sequence + 1, context.now, effect);
        if (transition == RelayResult::Invalid)
            return WorldActionResult::Failed;
        if (transition == RelayResult::Encoded && (version_ == UINT64_MAX || !result.owned.Append(effect.View())))
            return WorldActionResult::Failed;
        if (transition == RelayResult::Encoded)
            result.flight = next.Active() ? FlightEvent::Deploy : FlightEvent::Stop;
        glider_ = next;
        if (transition == RelayResult::Encoded)
            ++version_;
        if (skillChanged && transition != RelayResult::Encoded && result.peers.Size())
            result.peerEffectVersion = version_;
        if (motion.id == 16 && !reported_) {
            reported_ = true;
            readyAt_ = context.now;
        }
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (!skills_)
            return WorldActionResult::Ignored;
        if (version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        const auto result = skills_->Tick(context, out);
        if (result == WorldActionResult::Failed)
            return result;
        if (skills_->PeerEffectVersion() != skillsVersion_) {
            skillsVersion_ = skills_->PeerEffectVersion();
            ++version_;
            if (out.peers.Size())
                out.peerEffectVersion = version_;
        }
        return result;
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& context, const ReceivedWorldEffect& request,
                                            std::unique_ptr<PreparedWorldEffect>& prepared) override {
        if (!modern::MaximumReceivedEffectDuration<49284>(request.definition))
            return WorldActionResult::Ignored;
        return PrepareReceivedEffects(context, {request}, prepared);
    }
    WorldActionResult PrepareStun(const WorldActionContext& c, std::unique_ptr<PreparedWorldControl>& out) override {
        return skills_->PrepareStun(c, out);
    }
    WorldActionResult PrepareBanish(const WorldActionContext& c, uint64_t utc,
                                    std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 0, out);
    }
    WorldActionResult PrepareStability(const WorldActionContext& c, uint64_t utc,
                                       std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 1, out);
    }
    WorldActionResult PrepareDemonLeapImpact(const WorldActionContext& c, uint64_t utc,
                                             std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 5, out);
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t utc,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 4, out);
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t utc,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 6, out);
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t utc,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 3, out);
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t utc,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, utc, 2, out);
    }
    WorldActionResult PrepareHostileRemoval(const WorldActionContext& c, uint64_t utc, unsigned operation,
                                            std::unique_ptr<PreparedHostileEffects>& out) {
        if (!skills_ || version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        std::unique_ptr<PreparedHostileEffects> child;
        const auto result = operation == 6   ? skills_->PrepareBurstStrength(c, utc, child)
                            : operation == 5 ? skills_->PrepareDemonLeapImpact(c, utc, child)
                            : operation == 4 ? skills_->PrepareElementalBlast(c, utc, child)
                            : operation == 3 ? skills_->PrepareGazeDarkness(c, utc, child)
                            : operation == 2 ? skills_->PrepareEngagement(c, utc, child)
                            : operation == 1 ? skills_->PrepareStability(c, utc, child)
                                             : skills_->PrepareBanish(c, utc, child);
        if (result != WorldActionResult::Handled || !child)
            return result;
        struct Prepared final : PreparedHostileEffects {
            HotActions& owner;
            std::unique_ptr<PreparedHostileEffects> child;
            Prepared(HotActions& value, std::unique_ptr<PreparedHostileEffects> next)
                : owner(value), child(std::move(next)) {}
            void Commit() noexcept override {
                child->Commit();
                const auto version = owner.skills_->PeerEffectVersion();
                if (owner.skillsVersion_ != version) {
                    owner.skillsVersion_ = version;
                    ++owner.version_;
                }
            }
        };
        auto next = std::unique_ptr<Prepared>(new Prepared(*this, std::move(child)));
        next->controlBlocked = next->child->controlBlocked;
        next->beforeEffects = next->child->beforeEffects;
        next->afterEffects = next->child->afterEffects;
        next->beforeConditions = next->child->beforeConditions;
        next->afterConditions = next->child->afterConditions;
        next->beforeRemoved = next->child->beforeRemoved;
        next->afterRemoved = next->child->afterRemoved;
        next->output = std::move(next->child->output);
        next->output.peerEffectVersion = version_ + ((next->output.owned.Size() || next->output.peers.Size()) ? 1 : 0);
        out = std::move(next);
        return WorldActionResult::Handled;
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                             const std::vector<ReceivedWorldEffect>& requests,
                                             std::unique_ptr<PreparedWorldEffect>& prepared) override {
        if (!skills_)
            return WorldActionResult::Ignored;
        if (version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        std::unique_ptr<PreparedWorldEffect> child;
        const auto result = skills_->PrepareReceivedEffects(context, requests, child);
        if (result != WorldActionResult::Handled)
            return result;
        if (!child)
            return WorldActionResult::Failed;
        auto next = std::unique_ptr<Received>(new Received(*this, std::move(child)));
        next->before = next->child->before;
        next->after = next->child->after;
        next->instance = next->child->instance;
        next->instances = next->child->instances;
        next->output = std::move(next->child->output);
        next->output.peerEffectVersion = version_ + 1;
        prepared = std::move(next);
        return WorldActionResult::Handled;
    }
    bool PeerEffectsReady(uint64_t now) const override {
        return (skills_ && skills_->HasStarted()) || (reported_ && now >= readyAt_ && now - readyAt_ >= 200);
    }
    uint64_t PeerEffectVersion() const override {
        return version_;
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        return !skills_ || skills_->PeerCleanup(sequence, out);
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        if (context.peerAgent != agent_)
            return WorldActionResult::Failed;
        Buffer result;
        if (skills_) {
            const auto skill = skills_->PeerEffect(context, seen, result);
            if (skill == WorldActionResult::Failed)
                return skill;
        }
        if (glider_.Active() || seen) {
            Buffer glide;
            if (!GliderCommand(agent_, context.sequence, glider_.Active(), glide) || !result.Append(glide.View()))
                return WorldActionResult::Failed;
        }
        if (!result.Size())
            return WorldActionResult::Ignored;
        out = std::move(result);
        return WorldActionResult::Handled;
    }

  private:
    struct Received final : PreparedWorldEffect {
        HotActions& owner;
        std::unique_ptr<PreparedWorldEffect> child;
        Received(HotActions& target, std::unique_ptr<PreparedWorldEffect> value)
            : owner(target), child(std::move(value)) {}
        void Commit() noexcept override {
            child->Commit();
            owner.skillsVersion_ = owner.skills_->PeerEffectVersion();
            ++owner.version_;
        }
    };
    uint16_t agent_;
    GliderController glider_;
    uint64_t version_ = 0, readyAt_ = 0, skillsVersion_ = 0;
    bool reported_ = false;
    std::unique_ptr<StarterActions> skills_;
};
} // namespace
std::unique_ptr<WorldActions> CreateWorldActions(uint16_t agent) {
    if (agent < 257 || agent > 510)
        return {};
    return std::unique_ptr<WorldActions>(new HotActions(agent));
}
std::unique_ptr<WeaponActions> CreateWeaponActions(uint16_t agent, const Character& saved,
                                                   const Hot49284Loadout& recipe, uint8_t equipped,
                                                   std::string& error) {
    if (agent < 257 || agent > 510 || recipe.profession < 1 || recipe.profession > 9 || !saved.level || equipped > 63) {
        error = "Invalid HoT starter action context";
        return {};
    }
    auto skills = std::unique_ptr<StarterActions>(new StarterActions(agent, recipe, saved.level, equipped));
    if (!skills->LoadRecharges(saved, error))
        return {};
    error.clear();
    return std::unique_ptr<WeaponActions>(new HotActions(agent, std::move(skills)));
}
} // namespace hot49284
} // namespace glide
