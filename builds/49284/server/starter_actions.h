#include "glide/profile/demon_leap_impact.h"
#include "glide/profile/elemental_blast.h"
#include "glide/profile/gaze_darkness.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/boon_removal.h"
#pragma once
#include "glide/profile/dodge_evasion.h"
#include "starter_skills.h"
#include "glide/profile/beta_melee_combat.h"
#include "profession_state.h"
#include "self_effect.h"
#include "elementalist_healing.h"
#include "signet_malice_healing.h"
#include "hammers_rules.h"
#include "dwarf_healing.h"
#include "bubble_rules.h"
#include "magnetic_grasp.h"
#include "brand_rules.h"
#include "venomous_sphere_rules.h"
#include "engineer_projectile_rules.h"
#include "engineer_terminal_rules.h"
#include "glide/profile/modern_missile.h"
#include "glide/profile/modern_energy.h"
#include "glide/profile/dragon_release.h"
#include "glide/profile/endurance.h"
#include "glide/profile/initiative.h"
#include "skill_protocol.h"
#include "motion_state.h"
#include "world_entry.h"
#include "game.h"
#include "glide/character.h"
#include "glide/profile/world_actions.h"
#include <map>
#include <limits>

namespace glide {
namespace hot49284 {
// HoT keeps its wire format and animation phases. Local skill data supplies
// the recharge values used by these actions.
class StarterActions final : public WorldActions {
  public:
    StarterActions(uint16_t agent, const Hot49284Loadout& recipe, unsigned level, uint8_t equipped)
        : agent_(agent), recipe_(recipe), level_(level), equipped_(equipped), conditions_(agent),
          energy_(recipe.profession == 9, agent), hammers_(agent) {}
    // Pending land/water flights each use their own availability rule in Tick.
    void SetEquipment(uint8_t equipped) {
        equipped_ = equipped;
    }
    bool EvadesIncoming(uint64_t now) const override {
        return evasion_.Active(now);
    }
    uint64_t PeerEffectVersion() const override {
        return version_;
    }
    bool HasStarted() const {
        return started_;
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        if (context.peerAgent != agent_ || !context.sequence)
            return WorldActionResult::Failed;
        Buffer result;
        if (legendsReported_ && !LegendSelection(agent_, legends_, 0, result))
            return WorldActionResult::Failed;
        if (attunementReported_ && !AttunementSelection(agent_, attunement_, result))
            return WorldActionResult::Failed;
        if (mediumReported_ && !WeaponSelection(result))
            return WorldActionResult::Failed;
        if (!seen && !aura_.Snapshot(agent_, context.now, level_, result))
            return WorldActionResult::Failed;
        if (!seen && (!healingEffects_.Snapshot(agent_, context.now, level_, result) ||
                      !conditions_.Snapshot(agent_, context.wallNow, level_, result)))
            return WorldActionResult::Failed;
        if (!seen && !hammers_.Snapshot(agent_, context.now, level_, energy_, result))
            return WorldActionResult::Failed;
        if (!seen && !missiles_.Snapshot(49284, agent_, context.now, result))
            return WorldActionResult::Failed;
        if (!seen && !grasp_.Snapshot(agent_, context.now, result))
            return WorldActionResult::Failed;
        if (!seen && !brand_.Snapshot(agent_, context.now, result))
            return WorldActionResult::Failed;
        if (seen || plan_.empty() || !next_ || !ActionInRange(context) ||
            (skill_ == DemonLeap::Skill && context.now >= due_)) {
            if (!result.Size())
                return WorldActionResult::Ignored;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        auto stage = plan_[next_ - 1];
        stage.durationMs =
            static_cast<uint16_t>((std::min)(due_ > context.now ? due_ - context.now : 1, uint64_t(stage.durationMs)));
        auto movement = emittedMovement_;
        bool moving = emittedMoving_;
        if (skill_ == DemonLeap::Skill && stage.type == 1) {
            if (!DemonLeap::Aim(context, leapPoint_, medium_ == 1, stage.durationMs, movement.direction, movement.speed,
                                moving))
                return WorldActionResult::Failed;
            movement.facing = movement.direction;
        }
        if (!SkillPhase(agent_, context.sequence, identity_, skill_, slot_, target_, stage, true, next_ == plan_.size(),
                        moving ? &movement : nullptr, medium_ == 1, result))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool PeerCleanup(uint32_t, Buffer& out) const override {
        return aura_.Cleanup(agent_, out) && healingEffects_.Cleanup(agent_, out) && hammers_.Cleanup(agent_, out) &&
               missiles_.Cleanup(49284, out, agent_) && grasp_.Cleanup(agent_, out) && brand_.Cleanup(agent_, out);
    }
    bool LoadRecharges(const Character& saved, std::string& error) {
        const auto initiative = saved.profileFields.find("initiative_state");
        if (initiative != saved.profileFields.end() && !initiative->second.empty()) {
            modern::InitiativeLedger value;
            if (recipe_.profession != 5 || !value.Load(initiative->second)) {
                error = "Invalid saved HoT initiative";
                return false;
            }
        }
        const auto resource = saved.profileFields.find("endurance_state");
        if (resource != saved.profileFields.end() && !resource->second.empty()) {
            modern::EnduranceLedger value;
            if (!value.Load(resource->second)) {
                error = "Invalid saved HoT endurance";
                return false;
            }
        }
        if (!legends_.Load(saved, recipe_.profession)) {
            error = "Invalid saved HoT legends";
            return false;
        }
        if (!SavedAttunement(saved, recipe_.profession, attunement_)) {
            error = "Invalid saved HoT profession state";
            return false;
        }
        Character legacy;
        if (!hammers_.Load(saved, legacy) ||
            (recipe_.profession == 9 ? !healingEffects_.Load(legacy) : !aura_.Load(legacy, recipe_.profession))) {
            error = "Invalid saved HoT skill effects";
            return false;
        }
        std::map<uint32_t, uint64_t> next;
        const auto it = saved.profileFields.find("skill_recharges");
        if (it != saved.profileFields.end() && !it->second.empty()) {
            const auto& text = it->second;
            error = "Invalid saved HoT skill recharges";
            if (text.size() > 4096)
                return false;
            size_t begin = 0;
            while (begin < text.size()) {
                const auto comma = text.find(',', begin), end = comma == std::string::npos ? text.size() : comma,
                           colon = text.find(':', begin);
                uint64_t skill = 0, deadline = 0;
                if (colon == std::string::npos || colon >= end || !Decimal(text.substr(begin, colon - begin), skill) ||
                    skill > UINT32_MAX || !Decimal(text.substr(colon + 1, end - colon - 1), deadline) || !deadline ||
                    !Recharge(static_cast<uint32_t>(skill)) ||
                    !next.emplace(static_cast<uint32_t>(skill), deadline).second)
                    return false;
                if (comma == std::string::npos)
                    break;
                begin = comma + 1;
                if (begin == text.size())
                    return false;
            }
        }
        deadlines_ = std::move(next);
        restored_ = false;
        error.clear();
        return true;
    }
    WorldActionResult PrepareStun(const WorldActionContext& c, std::unique_ptr<PreparedWorldControl>& out) override {
        if (c.peerAgent != agent_ || !c.sequence)
            return WorldActionResult::Failed;
        const auto impact = c.controlImpactUtc ? c.controlImpactUtc : c.wallNow;
        if (!impact || impact > c.wallNow)
            return WorldActionResult::Failed;
        auto result = std::unique_ptr<PreparedWorldControl>(new PreparedWorldControl);
        result->output.sequences = 0;
        if (!plan_.empty() && (!SkillEnd(agent_, c.sequence, identity_, result->output.owned) ||
                               !SkillEnd(agent_, c.sequence, identity_, result->output.peers)))
            return WorldActionResult::Failed;
        result->commit = [this, impact] {
            field_.CancelIfPending(impact);
            release_.CancelIfPending(impact);
            plan_.clear();
            supportPhaseEnds_.clear();
            melee_ = {};
            pendingHeals_.clear();
            healingFinishAt_ = riteDue_ = riteUtc_ = approachDue_ = backstepDue_ = backstepUtc_ = stealthAttackDue_ =
                warpDue_ = 0;
            warpPending_ = false;
            absorption_ = {};
        };
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult PrepareDemonLeapImpact(const WorldActionContext& c, uint64_t nominal,
                                             std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::DemonLeapImpact::Conditions<49284>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t nominal,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ElementalBlast::Conditions<49284>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t nominal,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::BurstStrengthImpact::Conditions<49284>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t nominal,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::GazeDarkness::Conditions<49284>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 2, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t nominal,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ForcedEngagement::Conditions<49284>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, true, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareBanish(const WorldActionContext& c, uint64_t nominal,
                                    std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, nominal, false, out);
    }
    WorldActionResult PrepareStability(const WorldActionContext& c, uint64_t nominal,
                                       std::unique_ptr<PreparedHostileEffects>& out) override {
        return PrepareHostileRemoval(c, nominal, true, out);
    }
    WorldActionResult PrepareHostileRemoval(const WorldActionContext& c, uint64_t nominal, unsigned operation,
                                            std::unique_ptr<PreparedHostileEffects>& out) {
        const bool stability = operation == 1;
        if (operation > 3)
            return WorldActionResult::Failed;
        if (!nominal || nominal > c.wallNow)
            return WorldActionResult::Failed;

        if (c.peerAgent != agent_ || !c.wallNow || version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        struct Prepared final : PreparedHostileEffects {
            bool changed = false;
            StarterActions& owner;
            SelfEffect aura;
            DwarfHealingEffects healing;
            modern::Hammers<49284> hammers;
            modern::ConditionActions<49284> conditions;
            Prepared(StarterActions& value)
                : owner(value), aura(value.aura_), healing(value.healingEffects_), hammers(value.hammers_),
                  conditions(value.conditions_) {}
            void Commit() noexcept override {
                owner.aura_ = std::move(aura);
                owner.healingEffects_ = std::move(healing);
                owner.started_ = true;
                owner.hammers_ = std::move(hammers);
                owner.conditions_ = std::move(conditions);
                if (changed)
                    ++owner.version_;
            }
        };
        auto next = std::unique_ptr<Prepared>(new Prepared(*this));
        std::vector<combat::LiveBoon> live, removed;
        next->aura.Boons(live);
        next->healing.Boons(live);
        next->hammers.Boons(c, live);
        combat::BoonRemovals history;
        if (!history.Load(c.removedBoons) ||
            (operation < 2 && !(stability ? history.SelectContribution(live, 1122, c.wallNow, removed, nominal)
                                          : history.Select(live, combat::BanishEnchantment::RemovalLimit(49284),
                                                           c.wallNow, removed, nominal))))
            return WorldActionResult::Failed;
        const auto ids = combat::BoonRemovals::Instances(removed);
        next->aura.ForgetBoons(ids);
        next->healing.ForgetBoons(ids);
        const auto removedText = history.Text();
        if (!next->hammers.SetRemovedBoons(removedText))
            return WorldActionResult::Failed;
        next->controlBlocked = stability && !removed.empty();
        std::string conditions = c.conditionState;
        if (operation == 0 && !combat::BanishEnchantment::Conditions<49284>(c, nominal, conditions))
            return WorldActionResult::Failed;
        if (operation) {
            modern::ConditionState<49284> ordinary;
            if (!ordinary.Load(conditions))
                return WorldActionResult::Failed;
            ordinary.Expire(c.wallNow);
            conditions = ordinary.Text();
        }
        auto changed = c;
        changed.conditionState = conditions;
        modern::ConditionActions<49284>::Prepared prepared;
        WorldActionOutput output;
        output.sequences = 0;
        if (operation == 2 && nominal <= UINT64_MAX - combat::GazeDarkness::Duration &&
            nominal + combat::GazeDarkness::Duration > c.wallNow) {
            Buffer cleared;
            bool suppressed = false;
            if (!next->aura.SuppressStealth(agent_, cleared, suppressed) || !output.owned.Append(cleared.View()) ||
                !output.peers.Append(cleared.View()))
                return WorldActionResult::Failed;
        }

        if (!next->conditions.Prepare(changed, 0, level_, prepared, output) ||
            !combat::RemovedBoonMessages<49284>(agent_, c.peerAgent, removed, output.owned, output.peers))
            return WorldActionResult::Failed;
        next->conditions.Commit(std::move(prepared));
        std::string prefix;
        if (c.skillEffects.compare(0, 3, "H1:") == 0 || c.skillEffects.compare(0, 3, "H2:") == 0 ||
            c.skillEffects.compare(0, 3, "H3:") == 0) {
            const auto bar = c.skillEffects.find('|');
            if (bar == std::string::npos)
                return WorldActionResult::Failed;
            prefix = c.skillEffects.substr(0, bar + 1);
        }
        const auto effects = prefix + (recipe_.profession == 9 ? next->healing.Text() : next->aura.Text());
        if (effects.size() > 8192)
            return WorldActionResult::Failed;
        next->beforeEffects = c.skillEffects;
        next->afterEffects = effects;
        next->beforeConditions = c.conditionState;
        next->afterConditions = conditions;
        next->beforeRemoved = c.removedBoons;
        next->afterRemoved = removedText;
        next->changed = output.owned.Size() || output.peers.Size();
        output.peerEffectVersion = version_ + (next->changed ? 1 : 0);
        next->output = std::move(output);
        out = std::move(next);
        return WorldActionResult::Handled;
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& context, const ReceivedWorldEffect& request,
                                            std::unique_ptr<PreparedWorldEffect>& prepared) override {
        if (!modern::MaximumReceivedEffectDuration<49284>(request.definition))
            return WorldActionResult::Ignored;
        return PrepareReceivedEffects(context, {request}, prepared);
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                             const std::vector<ReceivedWorldEffect>& requests,
                                             std::unique_ptr<PreparedWorldEffect>& prepared) override {
        if (requests.empty() || requests.size() > 5 || context.peerAgent != agent_ || !context.wallNow ||
            version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        auto aura = aura_;
        auto healing = healingEffects_;
        if (!restored_ &&
            (!aura.Restore(context.now, context.wallNow) || !healing.Restore(context.now, context.wallNow)))
            return WorldActionResult::Failed;
        Buffer bytes;
        std::vector<unsigned> instances;
        for (const auto& request : requests) {
            unsigned instance = 0;
            if (!(recipe_.profession == 9 ? healing.Receive(agent_, context, request, level_, instance, bytes)
                                          : aura.Receive(agent_, context, request, level_, instance, bytes)))
                return WorldActionResult::Failed;
            instances.push_back(instance);
        }
        std::string prefix;
        if (context.skillEffects.compare(0, 3, "H1:") == 0 || context.skillEffects.compare(0, 3, "H2:") == 0 ||
            context.skillEffects.compare(0, 3, "H3:") == 0) {
            const auto bar = context.skillEffects.find('|');
            if (bar == std::string::npos)
                return WorldActionResult::Failed;
            prefix = context.skillEffects.substr(0, bar + 1);
        }
        auto next = std::unique_ptr<Received>(new Received(*this, std::move(aura), std::move(healing)));
        next->before = context.skillEffects;
        next->after = prefix + (recipe_.profession == 9 ? next->healing.Text() : next->aura.Text());
        next->instances = std::move(instances);
        next->instance = next->instances.front();
        next->output.sequences = 0;
        next->output.peerEffectVersion = version_ + 1;
        if (!next->output.owned.Append(bytes.View()) || !next->output.peers.Append(bytes.View()))
            return WorldActionResult::Failed;
        prepared = std::move(next);
        return WorldActionResult::Handled;
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        if (!MeleeDue(now) && !missiles_.CombatDue(now))
            return false;
        MotionMessage m{};
        size_t consumed = 0;
        return DecodeMotion(input, m, consumed) == DecodeStatus::Complete && consumed == input.size && m.id == 18;
    }
    WorldActionResult BasicAttackRequest(const WorldActionContext& c, uint16_t target, uint32_t token,
                                         Buffer& out) const override {
        if (!plan_.empty() || combatRecovery_ || MeleeDue(c.now))
            return WorldActionResult::Ignored;
        return combat::ForcedEngagement::BasicRequest(49284, agent_, target, 0, token, out) ? WorldActionResult::Handled
                                                                                            : WorldActionResult::Failed;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        if (combatRecovery_)
            return WorldActionResult::Failed;
        Reader request(input);
        uint16_t requestId = 0;
        if (request.U16(requestId) && requestId == 109) {
            uint8_t slot = 0, legend = 0;
            if (!request.U8(slot) || !request.U8(legend) || request.Remaining())
                return WorldActionResult::Failed;
            return ChangeLegend(input, context, false, slot, legend, 0, out);
        }
        MotionMessage message{};
        size_t consumed = 0;
        const auto decoded = DecodeMotion(input, message, consumed);
        if (decoded == DecodeStatus::Unknown)
            return WorldActionResult::Ignored;
        if (decoded != DecodeStatus::Complete || consumed != input.size)
            return WorldActionResult::Failed;
        if (message.id == 9 && message.input_size && message.input[0] == 0x51)
            return DodgeRequest(input, message, context, out);
        if (message.id == 16) {
            // C16 flags (state) remain owned by the glider. Its separate mode
            // selects deep water (3), surface (4), or land/air/glide (0/1/2/5).
            if (message.mode > 5)
                return WorldActionResult::Ignored;
            const uint8_t medium = message.mode == 3 ? 1 : message.mode == 4 ? 2 : 0;
            if (mediumReported_ && medium == medium_)
                return WorldActionResult::Ignored;
            if (context.peerAgent != agent_ || !context.sequence || context.sequence == UINT32_MAX ||
                version_ == UINT64_MAX)
                return WorldActionResult::Failed;
            if (!Restore(context))
                return WorldActionResult::Failed;
            auto next = *this;
            WorldActionOutput result;
            result.sequences = 0;
            if (next.legends_.Enabled() && next.legends_.Underwater() != (medium == 1)) {
                if (!next.legends_.Restore(context.now, context.wallNow))
                    return WorldActionResult::Failed;
                next.legends_.SetUnderwater(medium == 1);
                if (!LegendSelection(agent_, next.legends_, next.legends_.Remaining(context.now), result.owned) ||
                    !LegendSelection(agent_, next.legends_, 0, result.peers))
                    return WorldActionResult::Failed;
                next.legendsReported_ = true;
                // Changing medium preserves both saved pairs, swap deadline and energy.
            }
            if (medium == medium_ ||
                (!mediumReported_ && (next.hammers_.Tracked() || !next.pendingHeals_.empty() || next.riteDue_ ||
                                      next.approachDue_ || next.backstepDue_ || next.absorption_.Pending()))) {
                // Initial confirmation of the current medium does not interrupt
                // an action or clear its predecessor chain.
                next.medium_ = medium;
                if (!next.WeaponSelection(result.owned) || !next.WeaponSelection(result.peers))
                    return WorldActionResult::Failed;
                next.mediumReported_ = true;
                ++next.version_;
                *this = std::move(next);
                out = std::move(result);
                return WorldActionResult::Handled;
            }
            if (!next.plan_.empty()) {
                if (next.next_ < next.plan_.size() || Hammers::IsMaintainedSkill(next.skill_)) {
                    if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    result.sequences = 1;
                }
                if (next.recharge_) {
                    const auto ready = next.ready_.find(next.skill_);
                    const uint64_t remaining =
                        ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
                    if (!SkillbarRecharge(agent_, next.skill_,
                                          static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                          next.recharge_, result.owned))
                        return WorldActionResult::Failed;
                }
                next.plan_.clear();
            }
            next.medium_ = medium;
            next.mediumReported_ = true;
            next.previous_.clear();
            next.missiles_.CancelPending();
            next.warpPending_ = false;
            ++next.version_;
            next.pendingHeals_.clear();
            next.personalHealUtc_ = next.stealthAttackDue_ = 0;
            next.riteDue_ = next.riteUtc_ = 0;
            next.approachDue_ = 0;
            next.backstepDue_ = 0;
            next.absorption_.Cancel();
            next.release_.Cancel();
            if (!next.WeaponSelection(result.owned) || !next.WeaponSelection(result.peers))
                return WorldActionResult::Failed;
            if (next.hammers_.Present()) {
                if (!next.hammers_.Stop(context, next.energy_, result) || !context.ReserveSequences(result.sequences))
                    return WorldActionResult::Failed;
                // Stop the outer motion wrapper too: it must not relay a failed
                // medium transition while retaining the old maintained state.
                if (!next.SaveEffects(context))
                    return WorldActionResult::Failed;
            }
            *this = std::move(next);
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (message.id != 18)
            return WorldActionResult::Ignored;
        // Native capture 4ce279db: slot12 is the legend request. The separate
        // one-byte input18 also follows ordinary skills, so it cannot mean F1.
        const bool legendSwap =
            recipe_.profession == 9 && message.input_size == 6 && message.input[0] == 0x15 && message.input[5] == 12;
        const bool ground = message.input_size == 18 && message.input[0] == 0x16;
        if (!legendSwap && !ground && (message.input_size != 6 || message.input[0] != 0x15))
            return WorldActionResult::Ignored;
        if (message.agent != agent_ || context.peerAgent != agent_ || !context.sequence ||
            context.sequence >= UINT32_MAX - 1 || !Restore(context))
            return WorldActionResult::Failed;
        const uint32_t identity = message.input_token & 0x7fffffffu;
        if (!(message.input_token & 0x80000000u) || !identity)
            return WorldActionResult::Ignored;
        if (legendSwap) {
            uint32_t target = 0;
            std::memcpy(&target, message.input + 1, 4);
            if (target && target != agent_)
                return Empty(out);
            return ChangeLegend(input, context, true, 0, 0, identity, out);
        }
        if (!ground && recipe_.profession == 6 && message.input[5] >= 12 && message.input[5] <= 15) {
            const uint8_t selected = message.input[5] - 11;
            if (identity == lastIdentity_ || level_ < AttunementLevel(selected))
                return Empty(out);
            auto next = *this;
            WorldActionOutput result;
            result.sequences = 1;
            if (selected != attunement_) {
                if (version_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                if (!next.plan_.empty()) {
                    if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    ++result.sequences;
                    next.plan_.clear();
                    if (next.skill_ == ElementalistHealing::Skill) {
                        next.pendingHeals_.clear();
                        next.personalHealUtc_ = 0;
                    }
                    if (next.recharge_) {
                        const auto ready = next.ready_.find(next.skill_);
                        const uint64_t remaining =
                            ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
                        if (!SkillbarRecharge(agent_, next.skill_,
                                              static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                              next.recharge_, result.owned))
                            return WorldActionResult::Failed;
                    }
                }
                next.attunement_ = selected;
                next.attunementReported_ = true;
                next.previous_.clear();
                next.missiles_.CancelPending();
                ++next.version_;
                if (!AttunementSelection(agent_, selected, result.owned) ||
                    !AttunementSelection(agent_, selected, result.peers))
                    return WorldActionResult::Failed;
            }
            Buffer echo;
            if (OwnMotion(input, agent_, context.sequence + result.sequences - 1, echo) != RelayResult::Encoded ||
                !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()))
                return WorldActionResult::Failed;
            if (selected != attunement_) {
                if (!context.ReserveSequences(result.sequences))
                    return WorldActionResult::Failed;
                if (!context.persistProfessionState || !context.persistProfessionState(selected))
                    return Empty(out);
            }
            next.lastIdentity_ = identity;
            *this = std::move(next);
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (identity == lastIdentity_)
            return Empty(out);
        const auto slot = message.input[ground ? 17 : 5];
        const auto prior = context.now < chainUntil_ ? previous_.find(slot) : previous_.end();
        if (!ground && recipe_.profession == 5 && slot == ThiefHealing::Slot)
            return PersonalHealingRequest(input, context, identity,
                                          medium_ == 1 ? SignetMaliceHealing::Skill : ThiefHealing::Skill, out);
        if (!ground && recipe_.profession == 3 && slot == EngineerHealing::Slot)
            return PersonalHealingRequest(input, context, identity, EngineerHealing::Skill, out);
        if (!ground && recipe_.profession == 6 && slot == ElementalistHealing::Slot)
            return PersonalHealingRequest(input, context, identity, ElementalistHealing::Skill, out);
        if (ground && recipe_.profession == 9 && slot == DemonLeap::Slot && legends_.First() == DemonLeap::Legend) {
            uint32_t frame = 0;
            std::memcpy(&frame, message.input + 13, 4);
            if (frame)
                return Empty(out);
            std::array<float, 3> native;
            std::memcpy(native.data(), message.input + 1, 12);
            for (float coordinate : native)
                if (!std::isfinite(coordinate))
                    return Empty(out);
            const CatalogPosition destination{native[0] / 32, native[1] / 32, -native[2] / 32};
            return SupportSkillRequest(input, context, identity, DemonLeap::Skill, out, &destination);
        }
        if (ground && recipe_.profession == 9 && slot == InspiringReinforcement::Slot) {
            uint32_t frame = 0;
            std::memcpy(&frame, message.input + 13, 4);
            if (frame)
                return Empty(out);
            std::array<float, 3> native;
            std::memcpy(native.data(), message.input + 1, 12);
            for (float coordinate : native)
                if (!std::isfinite(coordinate))
                    return Empty(out);
            const CatalogPosition destination{native[0] / 32, native[1] / 32, -native[2] / 32};
            return SupportSkillRequest(input, context, identity, InspiringReinforcement::Skill, out, &destination);
        }
        if (!ground && recipe_.profession == 9 && slot <= 4 && legends_.First() == 11)
            return FacetRequest(input, context, identity, slot, out);
        if (!ground && recipe_.profession == 9 && slot == 2 && legends_.First() == 12)
            return HammerRequest(input, context, identity, out, 27107);
        if (!ground && recipe_.profession == 9 && slot == AssassinBackstep::Slot &&
            legends_.First() == AssassinBackstep::Legend)
            return SupportSkillRequest(input, context, identity, AssassinBackstep::Skill, out);
        if (!ground && recipe_.profession == 9 && slot == 4 && legends_.First() == 12)
            return SupportSkillRequest(input, context, identity, 28406, out);
        if (!ground && recipe_.profession == 9 && slot == 2 && legends_.First() == 13) {
            uint32_t target = 0;
            std::memcpy(&target, message.input + 1, 4);
            return SupportSkillRequest(input, context, identity, 26679, out, nullptr, target);
        }
        if (!ground && recipe_.profession == 9 && slot == 2 && legends_.First() == 14) {
            uint32_t target = 0;
            std::memcpy(&target, message.input + 1, 4);
            return SupportSkillRequest(input, context, identity, 27505, out, nullptr, target);
        }
        if (!ground && recipe_.profession == 9 && slot == Hammers::Slot)
            return HammerRequest(input, context, identity, out);
        if (!ground && recipe_.profession == 9 && slot == DwarfHealing::Slot) {
            const unsigned selected = legends_.First() == DwarfHealing::Legend   ? DwarfHealing::Skill
                                      : legends_.First() == DemonHealing::Legend ? DemonHealing::Skill
                                      : legends_.First() == 12                   ? 26937
                                                                                 : 0;
            return selected ? SupportSkillRequest(input, context, identity, selected, out) : Empty(out);
        }
        if (!ground && recipe_.profession == 9 && slot == AssassinApproach::Slot &&
            legends_.First() == AssassinApproach::Legend) {
            uint32_t target = 0;
            std::memcpy(&target, message.input + 1, 4);
            return SupportSkillRequest(input, context, identity, AssassinApproach::Skill, out, nullptr, target);
        }
        if (!ground && recipe_.profession == 9 && slot == PainAbsorptionAction::Rules::Slot)
            return SupportSkillRequest(input, context, identity, PainAbsorptionAction::Rules::Skill, out);
        if (!ground && recipe_.profession == 9 && slot == GreatDwarf::Slot)
            return legends_.First() == DemonElite::Legend
                       ? HammerRequest(input, context, identity, out, DemonElite::Skill)
                       : SupportSkillRequest(input, context, identity, GreatDwarf::Skill, out);
        const uint32_t predecessor = prior == previous_.end() ? 0 : prior->second;
        const BubbleRule* bubble = nullptr;
        for (const auto& rule : kBubbleRules)
            if (rule.slot == slot && BubbleAvailable(rule)) {
                bubble = &rule;
                break;
            }
        const bool flying = bubble && missiles_.CanDetonate(context.now, bubble->skill);
        const EngineerProjectileRule* engineer = nullptr;
        for (const auto& rule : kEngineerProjectileRules)
            if (rule.slot == slot && EngineerAvailable(rule)) {
                engineer = &rule;
                break;
            }
        const bool engineerActive = engineer && missiles_.CanDetonate(context.now, engineer->skill);
        const bool magnetic =
            slot == 7 && MagneticAvailable() && grasp_.Active(context.now) && grasp_.ValidTarget(context);
        const bool branded = slot == 8 && BrandAvailable() && brand_.Active(context.now) && brand_.ValidTarget(context);
        const bool venom =
            slot == 7 && VenomousSphereAvailable() && missiles_.CanDetonate(context.now, VenomousSphereRules::Base);
        const uint32_t activeEffect = venom            ? VenomousSphereRules::Marker
                                      : branded        ? BrandRules::Marker
                                      : magnetic       ? kMagneticMarker
                                      : engineerActive ? engineer->marker
                                      : flying         ? bubble->marker
                                                       : aura_.ActiveBuff(context.now);
        const auto* choice = SelectStarterSkill(recipe_, equipped_, level_, medium_ == 1 ? 24 : 29, slot, attunement_,
                                                predecessor, predecessor ? 1 : 0, activeEffect);
        if (!choice)
            return Empty(out);
        const auto* stance = StanceForSkill(choice->skill);
        const auto* followup = StanceForFollowup(choice->skill);
        const bool consumeStance = followup && choice->requiredEffect == aura_.ActiveBuff(context.now);
        const bool consumeBubble = flying && choice->skill == bubble->followup;
        const bool consumeGrasp = magnetic && choice->skill == kMagneticFollowup;
        const bool consumeEngineer = engineerActive && choice->skill == engineer->followup;
        const bool consumeBrand = branded && choice->skill == BrandRules::Followup;
        const bool consumeVenom = venom && choice->skill == VenomousSphereRules::Followup;
        const bool stealthAttack =
            choice->requiredEffect == ThiefHealing::Stealth && ThiefHealing::AttackActivation(choice->skill) != 0;
        if (!plan_.empty() && !(consumeStance && skill_ == followup->skill) &&
            !(consumeBubble && skill_ == bubble->skill) && !(consumeGrasp && skill_ == kMagneticBase) &&
            !(consumeEngineer && skill_ == engineer->skill) && !(consumeBrand && skill_ == BrandRules::Base) &&
            !(consumeVenom && skill_ == VenomousSphereRules::Base))
            return Empty(out);
        const auto* definition = StarterDefinitionFor(choice->skill);
        const auto* animation = StarterAnimationFor(choice->skill, medium_ == 1);
        const bool animationless = StarterAnimationless(choice->skill);
        if (!definition || definition->kind || definition->recharge < 0 || (!animation && !animationless))
            return WorldActionResult::Ignored;
        // HoT's own skillbar tests SkillDef+0x30 bit 0x1000 before choosing
        // the coordinate request. Do not treat a ground slot as a target ID.
        if (bool(definition->flags & 0x1000) != ground)
            return Empty(out);
        const auto ready = ready_.find(choice->skill);
        if (ready != ready_.end() && context.now < ready->second)
            return Empty(out);
        if (!energy_.CanSpend(choice->skill, context.now)) {
            WorldActionOutput notice;
            notice.sequences = 0;
            Guid sender;
            if (!ServiceMessages().ServiceChat("Not enough energy for this skill.", sender, notice.owned))
                return WorldActionResult::Failed;
            out = std::move(notice);
            return WorldActionResult::Handled;
        }
        auto next = *this;
        uint32_t duration = 0;
        WorldActionOutput cancelled;
        if ((consumeStance || consumeBubble || consumeGrasp || consumeEngineer || consumeBrand || consumeVenom) &&
            !next.plan_.empty()) {
            if (!SkillEnd(agent_, context.sequence, next.identity_, cancelled.owned) ||
                !SkillEnd(agent_, context.sequence, next.identity_, cancelled.peers))
                return WorldActionResult::Failed;
            cancelled.sequences = 1;
            next.plan_.clear();
            if (next.recharge_) {
                const auto oldReady = next.ready_.find(next.skill_);
                const uint64_t remaining = oldReady != next.ready_.end() && oldReady->second > context.now
                                               ? oldReady->second - context.now
                                               : 0;
                if (!SkillbarRecharge(agent_, next.skill_,
                                      static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                      next.recharge_, cancelled.owned))
                    return WorldActionResult::Failed;
            }
        }
        uint32_t target = 0;
        next.ground_ = ground;
        next.skill_ = choice->skill;
        if (ground) {
            uint32_t frame = 0;
            std::memcpy(&frame, message.input + 13, 4);
            // Coordinates relative to moving frames require that frame's
            // transform. Only the world frame is currently available.
            if (frame)
                return Empty(out);
            std::memcpy(next.groundPoint_.data(), message.input + 1, 12);
        } else
            std::memcpy(&target, message.input + 1, 4);
        // This aura affects its caster regardless of the UI's selected target.
        if (choice->skill == kAirAuraSkill || stance || followup || consumeBubble || consumeEngineer)
            target = 0;
        if (choice->skill == VenomousSphereRules::Base || consumeVenom)
            target = 0;
        if (consumeGrasp)
            target = grasp_.Target();
        if (consumeBrand)
            target = brand_.Target();
        if ((choice->skill == kMagneticBase || choice->skill == BrandRules::Base) && (!target || target == agent_))
            return Empty(out);
        next.target_ = target;
        if (!next.ActionInRange(context))
            return Empty(out);
        if (!animationless && !StarterPlan(*animation, next.plan_, duration))
            return WorldActionResult::Ignored;
        if (consumeGrasp) {
            CatalogPosition point;
            if (!context.characterTargetPosition || !context.characterTargetPosition(target, point))
                return Empty(out);
            const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y;
            const double distance = std::hypot(x, y) * 32;
            if (!std::isfinite(distance) || distance > kMagneticRange || next.plan_.size() != 1)
                return Empty(out);
            // The original phase is40ms. Travel time is an explicit local
            // adaptation, using the existing movement order and original phase.
            duration = static_cast<uint32_t>((std::max)(double(duration), std::ceil(distance * 1000 / kMagneticSpeed)));
            next.plan_[0].durationMs = static_cast<uint16_t>(duration);
        }
        if (target == agent_ && std::any_of(next.plan_.begin(), next.plan_.end(),
                                            [](const july14812::SkillStage& p) { return p.type != 0; }))
            return Empty(out);
        if (context.now > UINT64_MAX - duration - uint32_t(definition->recharge))
            return WorldActionResult::Failed;
        next.skill_ = choice->skill;
        next.identity_ = identity;
        next.slot_ = slot;
        next.target_ = target;
        next.next_ = 0;
        next.due_ = context.now;
        next.melee_ = {};
        if (beta::MeleeCombatSkill(49284, choice->skill) && !definition->recharge && target && context.combatTarget) {
            WorldCombatTarget enemy;
            if (context.combatTarget(target, enemy) && enemy.actor.agent == target && !enemy.actor.identity.empty()) {
                next.melee_.skill = choice->skill;
                next.melee_.identity = identity;
                next.melee_.target = target;
                next.melee_.targetIdentity = enemy.actor.identity;
                next.melee_.maximum = definition->maximumRange;
                next.melee_.medium = medium_;
            }
        }
        next.recharge_ = static_cast<uint32_t>(definition->recharge);
        next.lastIdentity_ = identity;
        if (stealthAttack) {
            const auto activation = ThiefHealing::AttackActivation(choice->skill);
            if (context.now > UINT64_MAX - activation)
                return WorldActionResult::Failed;
            next.stealthAttackDue_ = context.now + activation;
        }
        if (next.recharge_) {
            if (!context.wallNow || context.wallNow > UINT64_MAX - duration - next.recharge_)
                return WorldActionResult::Failed;
            for (auto entry = next.deadlines_.begin(); entry != next.deadlines_.end();)
                if (entry->second <= context.wallNow)
                    entry = next.deadlines_.erase(entry);
                else
                    ++entry;
            next.deadlines_[next.skill_] = context.wallNow + duration + next.recharge_;
            next.ready_[next.skill_] = context.now + duration + next.recharge_;
        }
        WorldActionOutput result, phase;
        const uint32_t prefix = cancelled.owned.Size() ? 1 : 0;
        result.sequences = (animationless ? 1 : 2) + prefix;
        Buffer echo;
        if (!result.owned.Append(cancelled.owned.View()) || !result.peers.Append(cancelled.peers.View()) ||
            OwnMotion(input, agent_, context.sequence + prefix, echo) != RelayResult::Encoded ||
            !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()) ||
            (!animationless && !next.Emit(context, context.sequence + prefix + 1, phase)) ||
            !result.owned.Append(phase.owned.View()) || !result.peers.Append(phase.peers.View()))
            return WorldActionResult::Failed;
        // The original client sends C18 even when its animation lookup returns
        // null, and creates no predicted order in that branch. Echo the input
        // and publish its private recharge without fabricating S24/S43 phases.
        if (animationless && next.recharge_ &&
            !SkillbarRecharge(agent_, next.skill_, next.recharge_, next.recharge_, result.owned))
            return WorldActionResult::Failed;
        const bool applyEffect = choice->skill == kAirAuraSkill || stance;
        const bool changeEffect = applyEffect || consumeStance;
        if (changeEffect) {
            Buffer buff;
            if (next.version_ == UINT64_MAX ||
                !(applyEffect ? next.aura_.Start(agent_, choice->skill, context.now, context.wallNow, level_, buff)
                              : next.aura_.Consume(agent_, buff)) ||
                !result.owned.Append(buff.View()) || !result.peers.Append(buff.View()))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (choice->skill == BrandRules::Base) {
            CatalogPosition point;
            if (!context.characterTargetPosition || !context.characterTargetPosition(target, point))
                return Empty(out);
            const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                         z = double(point.z) - context.position.z;
            const double distance = std::sqrt(x * x + y * y + z * z) * 32;
            if (!std::isfinite(distance) || distance <= 1 || distance > BrandRules::Range)
                return Empty(out);
            const unsigned travel = static_cast<unsigned>(std::ceil(distance * 1000 / BrandRules::Speed));
            modern::ControlledMissileGroup projectile; // Link is applied at estimated impact, not release.
            if (!next.brand_.Schedule(target, level_, context, travel) ||
                !next.missiles_.Schedule(BrandRules::Base, BrandRules::ReleaseMs, float(BrandRules::Range), target,
                                         context, level_, 49284, &projectile))
                return Empty(out);
        }
        if (consumeBrand) {
            Buffer buff;
            if (next.version_ == UINT64_MAX || context.now > UINT64_MAX - BrandRules::TriggerMs ||
                !next.brand_.Consume(context, buff) || !result.owned.Append(buff.View()) ||
                !result.peers.Append(buff.View()))
                return WorldActionResult::Failed;
            next.warpDue_ = context.now + BrandRules::TriggerMs;
            next.warpPending_ = true;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if ((choice->skill == 12510 || choice->skill == 13070) &&
            !next.missiles_.Schedule(choice->skill, static_cast<unsigned>(next.due_ - context.now),
                                     definition->maximumRange, target, context, level_, 49284))
            return Empty(out);
        if (bubble && choice->skill == bubble->skill &&
            !next.missiles_.Schedule(bubble->skill, bubble->launchMs, static_cast<float>(bubble->range), target,
                                     context, level_, 49284))
            return Empty(out);
        if (consumeBubble) {
            if (next.version_ == UINT64_MAX || !next.missiles_.Detonate(49284, context, result, bubble->skill))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (choice->skill == VenomousSphereRules::Base) {
            modern::ControlledMissileGroup sphere;
            sphere.marker = VenomousSphereRules::Marker;
            sphere.instance = VenomousSphereRules::Instance;
            sphere.appearanceSkill = VenomousSphereRules::Appearance;
            sphere.speed = float(VenomousSphereRules::Speed);
            if (!next.missiles_.Schedule(VenomousSphereRules::Base, VenomousSphereRules::ReleaseMs,
                                         float(VenomousSphereRules::Range), 0, context, level_, 49284, &sphere))
                return Empty(out);
        }
        if (consumeVenom) {
            if (next.version_ == UINT64_MAX ||
                !next.missiles_.Detonate(49284, context, result, VenomousSphereRules::Base))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (engineer && choice->skill == engineer->skill) {
            modern::ControlledMissileGroup group;
            group.marker = engineer->marker;
            group.instance = engineer->marker == 6075 ? UINT32_MAX - 3 : UINT32_MAX - 4;
            group.count = engineer->count;
            group.holdMs = engineer->holdMs;
            group.spreadRadians = engineer->spreadRadians;
            if (!next.missiles_.Schedule(engineer->skill, engineer->launchMs, static_cast<float>(engineer->range),
                                         target, context, level_, 49284, &group))
                return Empty(out);
        }
        if (consumeEngineer) {
            const auto* terminal = EngineerTerminalForSkill(engineer->skill);
            if (next.version_ == UINT64_MAX ||
                !next.missiles_.Detonate(49284, context, result, engineer->skill,
                                         terminal ? terminal->appearanceSkill : 0, terminal ? terminal->lifetimeMs : 0))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (choice->skill == kMagneticBase && !next.grasp_.Schedule(target, level_, context))
            return Empty(out);
        if (consumeGrasp) {
            Buffer buff;
            if (next.version_ == UINT64_MAX || !next.grasp_.Consume(context, buff) ||
                !result.owned.Append(buff.View()) || !result.peers.Append(buff.View()))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        modern::InitiativeLedger initiative;
        const bool initiativeEnabled = recipe_.profession == 5 && bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return WorldActionResult::Failed;
        if (initiativeEnabled) {
            unsigned cost = 0;
            if (!modern::InitialInitiativeCost(49284, choice->skill, cost) ||
                !initiative.Load(context.initiativeState) || !initiative.Advance(context.wallNow))
                return WorldActionResult::Failed;
            if (!initiative.Spend(cost))
                return Empty(out);
            if (!modern::InitiativeCurrent(49284, agent_, initiative, result.owned))
                return WorldActionResult::Failed;
        }
        bool hammersChanged = false;
        if (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, hammersChanged,
                                   false) ||
            !next.energy_.Spend(choice->skill, context.now, result))
            return WorldActionResult::Failed;
        if (hammersChanged) {
            if (next.version_ == UINT64_MAX)
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (initiativeEnabled || next.recharge_ || changeEffect || next.hammers_.Tracked()) {
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            const bool saved =
                initiativeEnabled
                    ? context.persistCombatInitiative(Text(next.deadlines_),
                                                      next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                      context.healthState, context.conditionState, initiative.Text())
                : (changeEffect || next.hammers_.Tracked())
                    ? next.SaveEffects(context)
                    : (context.persistSkillRecharges && context.persistSkillRecharges(Text(next.deadlines_)));
            if (!saved) {
                WorldActionOutput notice;
                notice.sequences = 0;
                Guid sender;
                if (!ServiceMessages().ServiceChat("The skill recharge could not be saved.", sender, notice.owned))
                    return WorldActionResult::Failed;
                out = std::move(notice);
                return WorldActionResult::Handled;
            }
        }
        if (initiativeEnabled)
            next.initiativeAnnounced_ = true;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (combatRecovery_ || context.peerAgent != agent_ || !context.sequence)
            return WorldActionResult::Failed;
        if (MeleeDue(context.now))
            return FinishMelee(context, out);
        const int projectile = missiles_.AdvanceCombat(49284, version_, context, out);
        if (projectile < 0) {
            combatRecovery_ = true;
            return WorldActionResult::Failed;
        }
        if (projectile == 0)
            return WorldActionResult::Handled;
        if (projectile == 2) {
            if (out.peerEffectVersion)
                version_ = out.peerEffectVersion;
            return WorldActionResult::Handled;
        }
        if (context.peerAgent != agent_ || !context.sequence || !Restore(context))
            return WorldActionResult::Failed;
        auto next = *this;
        WorldActionOutput result;
        result.sequences = 0;
        modern::InitiativeLedger initiative;
        const bool initiativeEnabled = recipe_.profession == 5 && bool(context.persistCombatInitiative);
        if ((!context.initiativeState.empty() && !initiativeEnabled) ||
            (initiativeEnabled && !initiative.Load(context.initiativeState)))
            return WorldActionResult::Failed;
        const auto previousInitiative = initiative.Points();
        if (initiativeEnabled && !initiative.Advance(context.wallNow))
            return WorldActionResult::Failed;
        bool initiativeSave = initiativeEnabled && (!initiativeAnnounced_ || context.initiativeState.empty() ||
                                                    initiative.Points() != previousInitiative);
        modern::EnduranceLedger resource;
        bool enduranceGain = false, enduranceSave = false;
        const bool enduranceEnabled =
            (recipe_.profession == 9 || enduranceAnnounced_ || !context.enduranceState.empty()) &&
            bool(context.persistCombatEndurance);
        if ((!context.enduranceState.empty() && !enduranceEnabled) ||
            (enduranceEnabled && (!resource.Load(context.enduranceState) || !resource.Advance(context.wallNow))))
            return WorldActionResult::Failed;
        if (next.legends_.Enabled() && !next.legendsReported_) {
            if (!next.legends_.Restore(context.now, context.wallNow) ||
                !LegendSelection(agent_, next.legends_, next.legends_.Remaining(context.now), result.owned))
                return WorldActionResult::Failed;
            next.legendsReported_ = true;
        }
        Buffer aura;
        bool auraChanged = false;
        bool suppressedStealth = false;
        if (context.revealed && !next.aura_.SuppressStealth(agent_, aura, suppressedStealth))
            return WorldActionResult::Failed;
        const bool energyDue = next.energy_.Due(context.now);
        bool hammersChanged = false;
        if (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, hammersChanged))
            return WorldActionResult::Failed;
        if (hammersChanged) {
            if (next.version_ == UINT64_MAX)
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (hammersChanged && !next.hammers_.Present() && Hammers::IsMaintainedSkill(next.skill_) &&
            !next.plan_.empty()) {
            if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
            next.plan_.clear();
        }
        if (!next.energy_.Advance(context.now, result))
            return WorldActionResult::Failed;
        bool invalidAttunement = false;
        for (const auto& rule : kStanceRules)
            if (rule.buff == next.aura_.ActiveBuff(context.now) && rule.professionState != 17 &&
                rule.professionState != attunement_)
                invalidAttunement = true;
        const bool cancelStance =
            next.aura_.ActiveStance(context.now) &&
            ((mediumReported_ && medium_ != 1) || !EquippedStarterWeapon(recipe_, equipped_, 24) || invalidAttunement);
        if (cancelStance) {
            if (!next.aura_.Consume(agent_, aura))
                return WorldActionResult::Failed;
            auraChanged = true;
            if (!next.plan_.empty()) {
                if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                    !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                    return WorldActionResult::Failed;
                result.sequences = 1;
                next.plan_.clear();
                if (next.recharge_) {
                    const auto ready = next.ready_.find(next.skill_);
                    const auto remaining =
                        ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
                    if (!SkillbarRecharge(agent_, next.skill_,
                                          static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                          next.recharge_, result.owned))
                        return WorldActionResult::Failed;
                }
            }
        } else if (!next.aura_.Advance(agent_, context.now, level_, aura, auraChanged))
            return WorldActionResult::Failed;
        auraChanged = auraChanged || suppressedStealth;
        if (auraChanged) {
            if (next.version_ == UINT64_MAX || !result.owned.Append(aura.View()) || !result.peers.Append(aura.View()))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        bool combatSave = false, regenerationSave = false, riteSave = false, riteFinished = false,
             absorptionSave = false, fieldSave = false, facetSave = false, releaseSave = false,
             approachRelocate = false;
        modern::GreatDwarfAlliesPlan ritePlan;
        modern::DragonReleasePlan releasePlan;
        modern::FacetPulsePlan facetPlan;
        std::vector<unsigned> facetOwnIds;
        CatalogPosition approachDestination;
        modern::InspiringFieldPlan fieldPlan;
        unsigned fieldOwnInstance = 0;
        modern::PainAbsorptionPlan absorptionPlan;
        modern::ConditionActions<49284>::Prepared conditions;
        std::string healthText = context.healthState;
        uint64_t assassinActivation = 0;
        if (recipe_.profession == 9) {
            float previousHealth = 0, currentHealth = 0;
            if (!modern::ReadHealthState(context.healthState, previousHealth, 100))
                return WorldActionResult::Failed;
            if (previousHealth == 0 || next.legends_.First() != InspiringReinforcement::Legend)
                next.field_.Cancel();
            if (previousHealth == 0 || next.legends_.First() != 11)
                next.release_.Cancel();
            if (previousHealth == 0 ||
                (IsSupportAction(next.skill_) && next.legends_.First() != SupportLegend(next.skill_))) {
                next.pendingHeals_.clear();
                next.riteDue_ = next.riteUtc_ = 0;
                next.approachDue_ = 0;
                next.backstepDue_ = 0;
                next.absorption_.Cancel();
                next.release_.Cancel();
                if (IsSupportAction(next.skill_) && !next.plan_.empty()) {
                    if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    result.sequences = 1;
                    next.plan_.clear();
                }
            }
            const bool healDue = modern::HealsDue(next.pendingHeals_, context.now);
            if (healDue && previousHealth > 0 && context.persistAssassinActivation)
                for (const auto& heal : next.pendingHeals_) {
                    if (heal.skill != 26937 || context.now < heal.startsAt)
                        continue;
                    const auto elapsed = context.now - heal.startsAt;
                    if (assassinActivation || elapsed >= context.wallNow)
                        return WorldActionResult::Failed;
                    assassinActivation = context.wallNow - elapsed;
                }
            const unsigned remove =
                modern::DwarfConditionRemovalDue(next.pendingHeals_, context, DwarfHealing::Removal, 100);
            std::vector<modern::PendingHeal> remaining;
            if (!modern::PrepareSelfHeals<49284>(next.pendingHeals_, context, agent_, remaining, previousHealth,
                                                 currentHealth, result, agent_, 100))
                return WorldActionResult::Failed;
            // Local ordering: heal using pre-existing condition kinds, then
            // apply the own demon self-condition in the same atomic commit.
            auto conditionContext = context;
            const bool backstepDue = next.backstepDue_ && context.now >= next.backstepDue_;
            if (backstepDue) {
                modern::ConditionState<49284> state;
                if (!context.wallNow || !state.Load(conditionContext.conditionState))
                    return WorldActionResult::Failed;
                state.RemoveDefinitions(AssassinBackstep::Definitions(), next.backstepUtc_);
                conditionContext.conditionState = state.Text();
                next.backstepDue_ = 0;
                if (enduranceEnabled) {
                    resource.Gain(AssassinBackstep::EnduranceGain());
                    enduranceGain = true;
                }
            }
            const bool eliteDue = next.hammers_.InitialConditionDue(context.now);
            if (eliteDue) {
                const auto start = next.hammers_.ActivationUtc();
                if (!start || context.wallNow < start)
                    return WorldActionResult::Failed;
                // Preserve the original deadline across late Tick and restoration.
                // Cancellation before this compound Tick cancels the pending action.
                if (context.wallNow - start < DemonElite::Duration) {
                    modern::ConditionState<49284> state;
                    if (!state.Load(conditionContext.conditionState))
                        return WorldActionResult::Failed;
                    state.Expire(context.wallNow);
                    if (!state.Add(DemonElite::SelfCondition, start, DemonElite::Duration))
                        return WorldActionResult::Failed;
                    conditionContext.conditionState = state.Text();
                }
                next.hammers_.AppliedInitialCondition();
            }
            if (healDue && previousHealth > 0) {
                for (const auto& heal : next.pendingHeals_)
                    if (heal.skill == DemonHealing::Skill && context.now >= heal.startsAt) {
                        const auto elapsed = context.now - heal.startsAt;
                        if (elapsed >= DemonHealing::SelfConditionDuration)
                            continue;
                        modern::ConditionState<49284> state;
                        if (!context.wallNow || context.wallNow <= elapsed ||
                            !state.Load(conditionContext.conditionState))
                            return WorldActionResult::Failed;
                        state.Expire(context.wallNow);
                        if (!state.Add(DemonHealing::SelfCondition, context.wallNow - elapsed,
                                       DemonHealing::SelfConditionDuration))
                            return WorldActionResult::Failed;
                        conditionContext.conditionState = state.Text();
                    }
            }
            if (next.absorption_.Due(context.now)) {
                std::string error;
                if (!next.absorption_.Prepare(conditionContext, absorptionPlan, error))
                    return Empty(out);
                conditionContext.conditionState = absorptionPlan.ownConditions;
                absorptionSave = true;
            }
            if (!next.conditions_.Prepare(conditionContext, remove, level_, conditions, result))
                return WorldActionResult::Failed;
            Buffer healingBuff;
            if (absorptionSave) {
                for (const auto& grant : absorptionPlan.grants)
                    if (!next.healingEffects_.GrantAbsorption(agent_, context, level_, grant, healingBuff))
                        return Empty(out);
                absorptionPlan.ownConditions = conditions.state.Text();
                next.absorption_.Cancel();
            }
            if (healDue && previousHealth > 0) {
                for (const auto& heal : next.pendingHeals_)
                    if (heal.skill == DwarfHealing::Skill && context.now >= heal.startsAt &&
                        !next.healingEffects_.Start(agent_, context, heal.startsAt, level_, healingBuff))
                        return WorldActionResult::Failed;
            }
            next.pendingHeals_ = std::move(remaining);
            const bool riteDue = next.riteDue_ && context.now >= next.riteDue_;
            if (next.approachDue_) {
                CatalogPosition unused;
                bool moving = false;
                if ((context.now >= next.approachDue_ &&
                     context.now - next.approachDue_ >= AssassinApproach::Duration) ||
                    !AssassinApproach::Destination(context, next.target_, next.medium_ == 1, unused, moving)) {
                    next.approachDue_ = 0;
                    if (!next.plan_.empty()) {
                        if (result.sequences >= UINT32_MAX - context.sequence ||
                            !SkillEnd(agent_, context.sequence + result.sequences, next.identity_, result.owned) ||
                            !SkillEnd(agent_, context.sequence + result.sequences, next.identity_, result.peers))
                            return WorldActionResult::Failed;
                        ++result.sequences;
                        next.plan_.clear();
                    }
                }
            }
            const bool approachDue = next.approachDue_ && context.now >= next.approachDue_;
            if (approachDue) {
                if (AssassinApproach::Destination(context, next.target_, next.medium_ == 1, approachDestination,
                                                  approachRelocate)) {
                    if (!next.healingEffects_.Start(agent_, context, next.approachDue_, level_, healingBuff,
                                                    AssassinApproach::Skill))
                        return WorldActionResult::Failed;
                } else if (!next.plan_.empty()) {
                    if (result.sequences >= UINT32_MAX - context.sequence ||
                        !SkillEnd(agent_, context.sequence + result.sequences, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence + result.sequences, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    ++result.sequences;
                    next.plan_.clear();
                }
                next.approachDue_ = 0;
            }
            bool effectChanged = false;
            if (!next.healingEffects_.Advance(agent_, context.now, level_, healingBuff, effectChanged) ||
                !result.owned.Append(healingBuff.View()) || !result.peers.Append(healingBuff.View()))
                return WorldActionResult::Failed;
            float regenerationDelta = 0;
            if (!next.healingEffects_.PrepareRegeneration(context.wallNow, currentHealth, 100, regenerationDelta,
                                                          regenerationSave))
                return WorldActionResult::Failed;
            if (regenerationDelta > 0 && (!modern::HealthChange<49284>(agent_, 718, regenerationDelta, result.owned) ||
                                          !modern::HealthChange<49284>(agent_, 718, regenerationDelta, result.peers)))
                return WorldActionResult::Failed;
            if (riteDue) {
                using Allies = modern::GreatDwarfAllies<49284>;
                auto status = Allies::Prepare(context, next.riteUtc_, {}, ritePlan);
                if (status == modern::GreatDwarfAlliesStatus::Expired) {
                    next.riteDue_ = next.riteUtc_ = 0;
                    riteFinished = true;
                } else if (status == modern::GreatDwarfAlliesStatus::Invalid)
                    return WorldActionResult::Failed;
                else if (status == modern::GreatDwarfAlliesStatus::Ready && !regenerationSave && !absorptionSave &&
                         !eliteDue && !healDue && !approachDue && !backstepDue && !conditions.save) {
                    std::vector<WorldConditionActor> allies;
                    if (!context.conditionAllies || !context.conditionAllies(allies))
                        return Empty(out);
                    if (Allies::Prepare(context, next.riteUtc_, allies, ritePlan) !=
                        modern::GreatDwarfAlliesStatus::Ready)
                        return WorldActionResult::Failed;
                    Buffer own;
                    if (!next.healingEffects_.Start(agent_, context, next.riteDue_, level_, own, GreatDwarf::Skill,
                                                    next.riteUtc_) ||
                        !result.owned.Append(own.View()) || !result.peers.Append(own.View()))
                        return WorldActionResult::Failed;
                    next.riteDue_ = next.riteUtc_ = 0;
                    riteSave = riteFinished = true;
                }
            }
            // Health/condition transactions take priority. Defer the field's
            // unchanged nominal pulse instead of making a second account write.
            if (next.field_.Active() && !regenerationSave && !absorptionSave && !eliteDue && !healDue && !riteDue &&
                !approachDue && !backstepDue && !conditions.save) {
                auto status = next.field_.Prepare(context, context.selfIdentity, {}, fieldPlan);
                if (status == modern::InspiringFieldStatus::Ready) {
                    std::vector<WorldConditionActor> allies;
                    if (!context.conditionAllies || !context.conditionAllies(allies))
                        return Empty(out);
                    status = next.field_.Prepare(context, context.selfIdentity, allies, fieldPlan);
                    if (status != modern::InspiringFieldStatus::Ready)
                        return WorldActionResult::Failed;
                    Buffer own;
                    if (fieldPlan.own && !next.healingEffects_.Receive(agent_, context, fieldPlan.ownEffect, level_,
                                                                       fieldOwnInstance, own))
                        return Empty(out);
                    if (!result.owned.Append(own.View()) || !result.peers.Append(own.View()))
                        return WorldActionResult::Failed;
                    if (own.Size()) {
                        if (next.version_ == UINT64_MAX)
                            return WorldActionResult::Failed;
                        next.started_ = true;
                        result.peerEffectVersion = ++next.version_;
                    }
                    fieldSave = true;
                } else if (status == modern::InspiringFieldStatus::Expired)
                    next.field_.Cancel();
                else if (status == modern::InspiringFieldStatus::Invalid)
                    return WorldActionResult::Failed;
            }
            if (next.release_.Due(context.now) && !regenerationSave && !fieldSave && !absorptionSave && !eliteDue &&
                !healDue && !riteDue && !approachDue && !backstepDue && !conditions.save) {
                auto status = next.release_.Prepare(context, {}, releasePlan);
                if (status == modern::DragonReleaseStatus::Expired)
                    next.release_.Cancel();
                else if (status == modern::DragonReleaseStatus::Ready) {
                    std::vector<WorldConditionActor> allies;
                    if (!context.conditionAllies || !context.conditionAllies(allies))
                        return Empty(out);
                    status = next.release_.Prepare(context, allies, releasePlan);
                    if (status != modern::DragonReleaseStatus::Ready || !releasePlan.own.empty())
                        return WorldActionResult::Failed;
                    releaseSave = true;
                } else if (status == modern::DragonReleaseStatus::Invalid)
                    return WorldActionResult::Failed;
            }
            if (next.hammers_.IsFacetMode() && !regenerationSave && !releaseSave && !fieldSave && !absorptionSave &&
                !eliteDue && !healDue && !riteDue && !approachDue && !backstepDue && !conditions.save) {
                auto status = next.hammers_.PrepareFacetPulse(context, {}, facetPlan);
                if (status == modern::FacetPulseStatus::Ready) {
                    std::vector<WorldConditionActor> allies;
                    if (!context.conditionAllies || !context.conditionAllies(allies))
                        return Empty(out);
                    status = next.hammers_.PrepareFacetPulse(context, allies, facetPlan);
                    if (status != modern::FacetPulseStatus::Ready)
                        return WorldActionResult::Failed;
                    Buffer own;
                    for (auto request : facetPlan.own) {
                        if (request.definition == 718)
                            request.sourceLevel = level_;
                        unsigned instance = 0;
                        if (!next.healingEffects_.Receive(agent_, context, request, level_, instance, own))
                            return Empty(out);
                        facetOwnIds.push_back(instance);
                    }
                    if (!result.owned.Append(own.View()) || !result.peers.Append(own.View()))
                        return WorldActionResult::Failed;
                    if (own.Size()) {
                        if (next.version_ == UINT64_MAX)
                            return WorldActionResult::Failed;
                        next.started_ = true;
                        result.peerEffectVersion = ++next.version_;
                    }
                    facetSave = true;
                } else if (status == modern::FacetPulseStatus::Invalid)
                    return WorldActionResult::Failed;
            }
            combatSave = regenerationSave || absorptionSave || eliteDue || healDue || riteFinished || approachDue ||
                         backstepDue || effectChanged || conditions.save;
            if ((healDue || regenerationSave) && !modern::WriteHealthState(currentHealth, healthText, 100))
                return WorldActionResult::Failed;
            if (absorptionSave || eliteDue || healDue || riteFinished || approachDue || backstepDue || effectChanged ||
                conditions.changed) {
                if (next.version_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                next.started_ = true;
                result.peerEffectVersion = ++next.version_;
            }
        } else {
            // Conditions can be received or removed by another player's
            // transaction, regardless of this recipient's profession.
            if (!next.conditions_.Prepare(context, 0, level_, conditions, result))
                return WorldActionResult::Failed;
            combatSave = conditions.save;
            if (conditions.changed) {
                if (next.version_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                next.started_ = true;
                result.peerEffectVersion = ++next.version_;
            }
        }
        if (recipe_.profession == 5 || recipe_.profession == 3 || recipe_.profession == 6) {
            float previous = 0, current = 0;
            if (!modern::ReadHealthState(context.healthState, previous, 100))
                return WorldActionResult::Failed;
            if (previous == 0) {
                next.pendingHeals_.clear();
                next.personalHealUtc_ = next.stealthAttackDue_ = 0;
                if ((next.skill_ == ThiefHealing::Skill || next.skill_ == SignetMaliceHealing::Skill ||
                     next.skill_ == EngineerHealing::Skill || next.skill_ == ElementalistHealing::Skill) &&
                    !next.plan_.empty()) {
                    if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    result.sequences = 1;
                    next.plan_.clear();
                }
            }
            const bool healDue = modern::HealsDue(next.pendingHeals_, context.now);
            std::vector<modern::PendingHeal> remaining;
            if (!modern::PrepareSelfHeals<49284>(next.pendingHeals_, context, agent_, remaining, previous, current,
                                                 result, agent_, 100))
                return WorldActionResult::Failed;
            if (healDue) {
                Buffer personal;
                if (previous > 0 && recipe_.profession != 6 && next.skill_ != SignetMaliceHealing::Skill &&
                    !(recipe_.profession == 3
                          ? next.aura_.GrantEngineerHealing(agent_, context, next.personalHealUtc_, level_, personal)
                          : next.aura_.GrantThiefHealing(agent_, context, next.personalHealUtc_, level_, personal)))
                    return WorldActionResult::Failed;
                if (!result.owned.Append(personal.View()) || !result.peers.Append(personal.View()) ||
                    !modern::WriteHealthState(current, healthText, 100) || next.version_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                next.pendingHeals_ = std::move(remaining);
                next.personalHealUtc_ = 0;
                combatSave = true;
                next.started_ = true;
                result.peerEffectVersion = ++next.version_;
            }
            if (next.stealthAttackDue_ && context.now >= next.stealthAttackDue_) {
                Buffer consumed;
                // Cancelled/invalid targets do not consume stealth. No hit or
                // Revealed duration is fabricated by this bounded adapter.
                if (next.ActionInRange(context) && next.aura_.ActiveBuff(context.now) == ThiefHealing::Stealth) {
                    if (!next.aura_.Consume(agent_, consumed) || !result.owned.Append(consumed.View()) ||
                        !result.peers.Append(consumed.View()) || next.version_ == UINT64_MAX)
                        return WorldActionResult::Failed;
                    result.peerEffectVersion = ++next.version_;
                }
                next.stealthAttackDue_ = 0;
            }
        }
        if (recipe_.profession != 9 && next.aura_.RegenerationEnabled()) {
            float currentHealth = 0, delta = 0;
            if (!modern::ReadHealthState(healthText, currentHealth, 100) ||
                !next.aura_.PrepareRegeneration(context.wallNow, currentHealth, 100, delta, regenerationSave))
                return WorldActionResult::Failed;
            if (regenerationSave) {
                combatSave = true;
                if (!modern::WriteHealthState(currentHealth, healthText, 100) ||
                    (delta > 0 && (!modern::HealthChange<49284>(agent_, 718, delta, result.owned) ||
                                   !modern::HealthChange<49284>(agent_, 718, delta, result.peers))))
                    return WorldActionResult::Failed;
            }
        }
        for (const auto& entry : next.resume_) {
            const auto ready = next.ready_.find(entry.first);
            const auto remaining =
                ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
            if (!SkillbarRecharge(agent_, entry.first,
                                  static_cast<uint32_t>((std::min)(remaining, uint64_t(entry.second))), entry.second,
                                  result.owned))
                return WorldActionResult::Failed;
        }
        next.resume_.clear();
        if (IsSupportAction(next.skill_) && !next.plan_.empty() && context.now >= next.healingFinishAt_) {
            // A delayed tick cannot stretch an authored support cast or replay
            // elapsed phases. Activation was handled above at its own deadline.
            if (next.next_ < next.plan_.size()) {
                if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                    !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                    return WorldActionResult::Failed;
                result.sequences = 1;
            }
            if (next.recharge_) {
                const auto ready = next.ready_.find(next.skill_);
                const auto remaining =
                    ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
                if (!SkillbarRecharge(agent_, next.skill_,
                                      static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                      next.recharge_, result.owned))
                    return WorldActionResult::Failed;
            }
            next.plan_.clear();
        }
        if (!next.plan_.empty() &&
            (context.now >= next.due_ || (next.skill_ == kMagneticFollowup && !next.ActionInRange(context)))) {
            const bool finished = next.next_ == next.plan_.size();
            const bool cancelled = (!finished || next.skill_ == kMagneticFollowup) && !next.ActionInRange(context);
            if (finished || cancelled) {
                if (cancelled) {
                    if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                        !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                        return WorldActionResult::Failed;
                    result.sequences = 1;
                    next.previous_.erase(next.slot_);
                    next.approachDue_ = 0;
                } else {
                    if (context.now > UINT64_MAX - 6000)
                        return WorldActionResult::Failed;
                    next.previous_[next.slot_] = next.skill_;
                    next.chainUntil_ = context.now + 6000;
                }
                if (next.recharge_) {
                    const auto ready = next.ready_.find(next.skill_);
                    const auto remaining =
                        ready != next.ready_.end() && ready->second > context.now ? ready->second - context.now : 0;
                    if (!SkillbarRecharge(agent_, next.skill_,
                                          static_cast<uint32_t>((std::min)(remaining, uint64_t(next.recharge_))),
                                          next.recharge_, result.owned))
                        return WorldActionResult::Failed;
                }
                next.plan_.clear();
            } else {
                if (IsSupportAction(next.skill_)) {
                    while (next.next_ < next.plan_.size() && context.now >= next.supportPhaseEnds_[next.next_])
                        ++next.next_;
                    if (next.next_ >= next.plan_.size())
                        return WorldActionResult::Failed;
                    next.plan_[next.next_].durationMs =
                        static_cast<uint16_t>(next.supportPhaseEnds_[next.next_] - context.now);
                }
                WorldActionOutput phase;
                if (!next.Emit(context, context.sequence, phase) || !result.owned.Append(phase.owned.View()) ||
                    !result.peers.Append(phase.peers.View()))
                    return WorldActionResult::Failed;
                result.sequences = 1;
            }
        }
        for (auto entry = next.ready_.begin(); entry != next.ready_.end();) {
            if (context.now < entry->second) {
                ++entry;
                continue;
            }
            if (!SkillbarRecharge(agent_, entry->first, 0, next.Recharge(entry->first), result.owned))
                return WorldActionResult::Failed;
            entry = next.ready_.erase(entry);
        }
        const auto available = [&](uint32_t skill) {
            if (skill == 12510)
                return next.recipe_.profession == 4 && next.medium_ == 0 &&
                       EquippedStarterWeapon(next.recipe_, next.equipped_, 29);
            if (skill == 13070)
                return next.recipe_.profession == 5 && next.medium_ == 1 &&
                       EquippedStarterWeapon(next.recipe_, next.equipped_, 24);
            if (skill == BrandRules::Base)
                return next.BrandAvailable();
            if (skill == VenomousSphereRules::Base)
                return next.VenomousSphereAvailable();
            const auto* rule = BubbleForSkill(skill);
            if (rule)
                return next.BubbleAvailable(*rule);
            const auto* engineer = EngineerProjectileForSkill(skill);
            return engineer && next.EngineerAvailable(*engineer);
        };
        if (next.missiles_.Due(context.now, available)) {
            const auto before = result.peers.Size();
            if (next.version_ == UINT64_MAX || !next.missiles_.Advance(49284, context, available, result))
                return WorldActionResult::Failed;
            if (result.peers.Size() != before) {
                next.started_ = true;
                result.peerEffectVersion = ++next.version_;
            }
        }
        Buffer grasp;
        if (!next.grasp_.Advance(context, next.MagneticAvailable(), grasp))
            return WorldActionResult::Failed;
        if (grasp.Size()) {
            if (next.version_ == UINT64_MAX || !result.owned.Append(grasp.View()) || !result.peers.Append(grasp.View()))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        Buffer brand;
        if (!next.brand_.Advance(context, next.BrandAvailable(), brand))
            return WorldActionResult::Failed;
        if (brand.Size()) {
            if (next.version_ == UINT64_MAX || !result.owned.Append(brand.View()) || !result.peers.Append(brand.View()))
                return WorldActionResult::Failed;
            next.started_ = true;
            result.peerEffectVersion = ++next.version_;
        }
        if (next.warpPending_) {
            if (!next.BrandAvailable() || !next.TargetInRange(next.target_, BrandRules::Followup, context))
                next.warpPending_ = false;
            else if (context.now >= next.warpDue_) {
                CatalogPosition destination;
                Buffer synchronization;
                if (!context.characterTargetPosition || !context.characterTargetPosition(next.target_, destination) ||
                    result.sequences >= UINT32_MAX - context.sequence ||
                    !SyncPosition(destination, agent_, context.sequence + result.sequences, synchronization) ||
                    !result.owned.Append(synchronization.View()) || !result.peers.Append(synchronization.View()))
                    return WorldActionResult::Failed;
                ++result.sequences;
                result.relocate = true;
                result.destination = destination;
                next.warpPending_ = false;
            }
        }
        // Completion is a local approach policy. No replay is persisted; only
        // the personal marker survives reconnect. Save the whole candidate
        // below before publishing either teleport or marker.
        if (approachRelocate) {
            Buffer synchronization;
            if (result.relocate || result.sequences >= UINT32_MAX - context.sequence ||
                !SyncPosition(approachDestination, agent_, context.sequence + result.sequences, synchronization) ||
                !result.owned.Append(synchronization.View()) || !result.peers.Append(synchronization.View()))
                return WorldActionResult::Failed;
            ++result.sequences;
            result.relocate = true;
            result.destination = approachDestination;
        }
        const bool receivedAuraSave = next.aura_.ReceivedText() != aura_.ReceivedText();
        // Refill is deferred while another multi-account transaction owns this
        // tick. Backstep cannot share those other legends' pending actions.
        const bool otherTransaction = riteSave || releaseSave || facetSave || fieldSave || absorptionSave;
        if (initiativeSave && otherTransaction)
            initiativeSave = false;
        if (initiativeSave && !modern::InitiativeCurrent(49284, agent_, initiative, result.owned))
            return WorldActionResult::Failed;
        if (enduranceGain && otherTransaction)
            return WorldActionResult::Failed;
        if (enduranceEnabled && !otherTransaction &&
            (!next.enduranceAnnounced_ || enduranceGain || resource.Points() != next.endurancePoints_)) {
            if (!modern::EnduranceCurrent(49284, agent_, resource.Points(), result.owned))
                return WorldActionResult::Failed;
            next.enduranceAnnounced_ = true;
            next.endurancePoints_ = resource.Points();
            enduranceSave = resource.Text() != context.enduranceState;
        }
        if (initiativeSave || enduranceSave || riteSave || releaseSave || facetSave || fieldSave || combatSave ||
            receivedAuraSave || cancelStance ||
            (next.hammers_.Tracked() && (energyDue || hammersChanged || auraChanged))) {
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            if (riteSave) {
                std::vector<unsigned> allocated;
                const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
                    if (ids.size() != ritePlan.allies.size())
                        return false;
                    for (auto id : ids)
                        if (!id || id >= modern::kFirstConditionInstance)
                            return false;
                    effects = next.hammers_.Text(next.EffectText(), next.energy_, context);
                    return !effects.empty() && effects.size() <= 8192;
                };
                const auto status = context.persistPreparedAlliedEffects
                                        ? context.persistPreparedAlliedEffects(Text(next.deadlines_), prepareOwn,
                                                                               ritePlan.allies, allocated)
                                        : WorldConditionCommit::NotApplied;
                if (status == WorldConditionCommit::RecoveryRequired)
                    return WorldActionResult::Failed;
                if (status != WorldConditionCommit::Applied)
                    return Empty(out);
            } else if (releaseSave) {
                std::vector<unsigned> allocated;
                const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
                    if (ids.size() != releasePlan.allies.size())
                        return false;
                    for (auto id : ids)
                        if (!id || id >= modern::kFirstConditionInstance)
                            return false;
                    effects = next.hammers_.Text(next.EffectText(), next.energy_, context);
                    return effects.size() <= 8192;
                };
                const auto status = context.persistPreparedAlliedEffects
                                        ? context.persistPreparedAlliedEffects(Text(next.deadlines_), prepareOwn,
                                                                               releasePlan.allies, allocated)
                                        : WorldConditionCommit::NotApplied;
                if (status == WorldConditionCommit::RecoveryRequired)
                    return WorldActionResult::Failed;
                if (status != WorldConditionCommit::Applied)
                    return Empty(out);
                next.release_.Cancel();
            } else if (facetSave) {
                std::vector<unsigned> allocated;
                const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
                    if (!next.hammers_.CommitFacetPulse(facetPlan, facetOwnIds, ids))
                        return false;
                    effects = next.hammers_.Text(next.EffectText(), next.energy_, context);
                    return effects.size() <= 8192;
                };
                const auto status = context.persistPreparedAlliedEffects
                                        ? context.persistPreparedAlliedEffects(Text(next.deadlines_), prepareOwn,
                                                                               facetPlan.allies, allocated)
                                        : WorldConditionCommit::NotApplied;
                if (status == WorldConditionCommit::RecoveryRequired)
                    return WorldActionResult::Failed;
                if (status != WorldConditionCommit::Applied)
                    return Empty(out);
            } else if (fieldSave) {
                std::vector<unsigned> allocated;
                const auto status =
                    context.persistAlliedEffects
                        ? context.persistAlliedEffects(Text(next.deadlines_),
                                                       next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                       fieldPlan.allies, allocated)
                        : WorldConditionCommit::NotApplied;
                if (status == WorldConditionCommit::RecoveryRequired)
                    return WorldActionResult::Failed;
                if (status != WorldConditionCommit::Applied)
                    return Empty(out);
                if (!next.field_.Commit(fieldPlan, allocated, fieldOwnInstance))
                    return WorldActionResult::Failed;
            } else if (absorptionSave) {
                const auto status = PainAbsorptionAction::Persist(
                    context, Text(next.deadlines_), next.hammers_.Text(next.EffectText(), next.energy_, context),
                    healthText, absorptionPlan);
                if (status == WorldConditionCommit::RecoveryRequired)
                    return WorldActionResult::Failed;
                if (status != WorldConditionCommit::Applied)
                    return Empty(out);
            } else {
                const auto conditionsText = conditions.state.Text();
                const bool saved =
                    assassinActivation
                        ? context.persistAssassinActivation(
                              Text(next.deadlines_), next.hammers_.Text(next.EffectText(), next.energy_, context),
                              healthText, conditionsText, enduranceSave ? resource.Text() : context.enduranceState,
                              assassinActivation)
                    : initiativeSave && enduranceSave
                        ? (context.persistCombatResources &&
                           context.persistCombatResources(
                               Text(next.deadlines_), next.hammers_.Text(next.EffectText(), next.energy_, context),
                               healthText, conditionsText, resource.Text(), initiative.Text()))
                    : initiativeSave
                        ? context.persistCombatInitiative(Text(next.deadlines_),
                                                          next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                          healthText, conditionsText, initiative.Text())
                    : enduranceSave
                        ? (context.persistCombatEndurance &&
                           context.persistCombatEndurance(Text(next.deadlines_),
                                                          next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                          healthText, conditionsText, resource.Text()))
                    : combatSave
                        ? (context.persistCombatConditions &&
                           context.persistCombatConditions(Text(next.deadlines_),
                                                           next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                           healthText, conditionsText))
                        : next.SaveEffects(context);
                if (!saved)
                    return Empty(out);
            }
        }
        next.conditions_.Commit(std::move(conditions));
        if (initiativeSave)
            next.initiativeAnnounced_ = true;
        *this = std::move(next);
        // A full-health heal can persist after reserving zero sequences without
        // emitting packets. Publish that zero count too: the caller's legacy
        // default is two, which would violate the completed reservation.
        const bool emitted = result.owned.Size() || result.peers.Size();
        if (!result.peers.Size())
            result.peerEffectVersion = 0;
        out = std::move(result);
        return emitted ? WorldActionResult::Handled : WorldActionResult::Ignored;
    }

  private:
    WorldActionResult DodgeRequest(ByteView input, const MotionMessage& message, const WorldActionContext& context,
                                   WorldActionOutput& out) {
        const auto token = message.input_token & 0x7fffffffu;
        float health = 0;
        if (!combat::DodgeEvasion::CanBegin(context.now) || message.agent != agent_ || context.peerAgent != agent_ ||
            !context.sequence || context.sequence > UINT32_MAX - 3 || message.state != 3 || message.input_size != 3 ||
            message.input[1] || message.input[2] || !(message.input_token & 0x80000000u) || !token ||
            token <= lastDodgeToken_ || !context.persistCombatEndurance ||
            (backstepDue_ && context.now >= backstepDue_))
            return Empty(out);
        if (!modern::ReadHealthState(context.healthState, health, 100))
            return WorldActionResult::Failed;
        if (!health)
            return Empty(out);
        if (!Restore(context))
            return WorldActionResult::Failed;
        modern::EnduranceLedger resource;
        if (!resource.Load(context.enduranceState) || !resource.Advance(context.wallNow))
            return WorldActionResult::Failed;
        if (!resource.SpendDodge())
            return Empty(out);
        auto next = *this;
        WorldActionOutput result;
        result.sequences = 2;
        if (!next.plan_.empty()) {
            if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                return WorldActionResult::Failed;
            ++result.sequences;
            next.plan_.clear();
        }
        const auto sequence = context.sequence + result.sequences - 2;
        Buffer relay;
        if (OwnMotion(input, agent_, sequence, relay) == RelayResult::Invalid || !result.owned.Append(relay.View()) ||
            !result.peers.Append(relay.View()) ||
            !modern::EnduranceCurrent(49284, agent_, resource.Points(), result.owned))
            return WorldActionResult::Failed;
        next.pendingHeals_.clear();
        next.personalHealUtc_ = next.stealthAttackDue_ = 0;
        next.riteDue_ = next.riteUtc_ = next.approachDue_ = next.backstepDue_ = 0;
        next.absorption_.Cancel();
        next.release_.Cancel();
        next.missiles_.CancelPending();
        next.warpPending_ = false;
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!context.persistCombatEndurance(Text(next.deadlines_),
                                            next.hammers_.Text(next.EffectText(), next.energy_, context),
                                            context.healthState, context.conditionState, resource.Text()))
            return Empty(out);
        next.evasion_.Begin(context.now);
        next.lastDodgeToken_ = token;
        next.endurancePoints_ = resource.Points();
        next.enduranceAnnounced_ = true;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    struct Received final : PreparedWorldEffect {
        StarterActions& owner;
        SelfEffect aura;
        DwarfHealingEffects healing;
        Received(StarterActions& target, SelfEffect a, DwarfHealingEffects h)
            : owner(target), aura(std::move(a)), healing(std::move(h)) {}
        void Commit() noexcept override {
            owner.aura_ = std::move(aura);
            owner.healingEffects_ = std::move(healing);
            owner.started_ = true;
            ++owner.version_;
        }
    };
    std::string EffectText() const {
        return recipe_.profession == 9 ? healingEffects_.Text() : aura_.Text();
    }
    bool SaveEffects(const WorldActionContext& context) const {
        return context.persistSkillState &&
               context.persistSkillState(Text(deadlines_), hammers_.Text(EffectText(), energy_, context));
    }
    WorldActionResult PersonalHealingRequest(ByteView input, const WorldActionContext& context, uint32_t identity,
                                             unsigned skill, WorldActionOutput& out) {
        float health = 0;
        if (!modern::ReadHealthState(context.healthState, health, 100))
            return WorldActionResult::Failed;
        const bool engineer = skill == EngineerHealing::Skill, elementalist = skill == ElementalistHealing::Skill,
                   signet = skill == SignetMaliceHealing::Skill;
        if ((!elementalist && ((engineer || signet) ? medium_ != 1 : medium_ == 1)) || !health || !level_ ||
            level_ > 80 || !plan_.empty() || !pendingHeals_.empty() || version_ == UINT64_MAX)
            return Empty(out);
        const unsigned animation = signet         ? SignetMaliceHealing::Animation
                                   : elementalist ? ElementalistHealing::Animation(medium_ == 1)
                                   : engineer     ? EngineerHealing::Animation
                                                  : ThiefHealing::Animation;
        const unsigned activation = signet         ? SignetMaliceHealing::Activation
                                    : elementalist ? ElementalistHealing::Activation(medium_ == 1)
                                    : engineer     ? EngineerHealing::Activation
                                                   : ThiefHealing::Activation;
        const unsigned recharge = signet         ? SignetMaliceHealing::Recharge
                                  : elementalist ? ElementalistHealing::Recharge
                                  : engineer     ? EngineerHealing::Recharge
                                                 : ThiefHealing::Recharge;
        const auto ready = ready_.find(skill);
        if (ready != ready_.end() && context.now < ready->second)
            return Empty(out);
        const uint64_t total = uint64_t(animation) + recharge;
        if (!context.wallNow || context.now > UINT64_MAX - total || context.wallNow > UINT64_MAX - total)
            return WorldActionResult::Failed;
        auto next = *this;
        WorldActionOutput result, phase;
        Buffer echo;
        july14812::SkillStage stage{};
        stage.durationMs = static_cast<uint16_t>(animation);
        next.plan_.assign(1, stage);
        next.skill_ = skill;
        next.slot_ = 0;
        next.target_ = 0;
        next.ground_ = false;
        next.identity_ = identity;
        next.next_ = 0;
        next.due_ = context.now;
        next.recharge_ = recharge;
        next.deadlines_[skill] = context.wallNow + total;
        next.ready_[skill] = context.now + total;
        modern::PendingHeal heal;
        heal.startsAt = context.now + activation;
        heal.skill = skill;
        heal.amount = signet         ? SignetMaliceHealing::Amount(level_)
                      : elementalist ? ElementalistHealing::Amount(level_)
                      : engineer     ? EngineerHealing::Amount(level_)
                                     : ThiefHealing::Amount(level_);
        next.pendingHeals_.push_back(heal);
        next.personalHealUtc_ = context.wallNow + activation;
        if (OwnMotion(input, agent_, context.sequence, echo) != RelayResult::Encoded ||
            !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()) ||
            !next.Emit(context, context.sequence + 1, phase) || !result.owned.Append(phase.owned.View()) ||
            !result.peers.Append(phase.peers.View()))
            return WorldActionResult::Failed;
        result.sequences = 2;
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!next.SaveEffects(context))
            return Empty(out);
        next.lastIdentity_ = identity;
        next.started_ = true;
        result.peerEffectVersion = ++next.version_;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult SupportSkillRequest(ByteView input, const WorldActionContext& context, uint32_t identity,
                                          unsigned selected, WorldActionOutput& out,
                                          const CatalogPosition* destination = nullptr, uint32_t target = 0) {
        float health = 0;
        if (!modern::ReadHealthState(context.healthState, health, 100))
            return WorldActionResult::Failed;
        const bool breakTaunt = context.taunted && selected == AssassinBackstep::Skill;
        if (!IsSupportAction(selected) || legends_.First() != SupportLegend(selected) || !level_ || level_ > 80 ||
            health == 0 || (!plan_.empty() && !breakTaunt) || !pendingHeals_.empty() || riteDue_ || approachDue_ ||
            backstepDue_ || absorption_.Pending() || version_ >= UINT64_MAX - 1)
            return Empty(out);
        const auto ready = ready_.find(selected);
        if (ready != ready_.end() && context.now < ready->second)
            return Empty(out);
        const bool assassin = selected == 26937;
        const bool engagement = selected == 26679, jade = selected == 28406, banish = selected == 27505;
        if (jade && medium_ == 1)
            return Empty(out);
        const bool leap = selected == DemonLeap::Skill, backstep = selected == AssassinBackstep::Skill,
                   approach = selected == AssassinApproach::Skill, field = selected == InspiringReinforcement::Skill,
                   demon = selected == DemonHealing::Skill, rite = selected == GreatDwarf::Skill,
                   absorption = selected == PainAbsorptionAction::Rules::Skill;
        if (banish || engagement) {
            WorldCombatTarget enemy;
            if (!target || !context.combatTarget || !context.combatTarget(target, enemy))
                return Empty(out);
            const auto& p = enemy.actor.position;
            const double distance =
                32 * std::hypot(std::hypot(double(p.x) - context.position.x, double(p.y) - context.position.y),
                                double(p.z) - context.position.z);
            if (!std::isfinite(distance) || distance > (engagement ? 1200 : 600))
                return Empty(out);
        }
        CatalogPosition approachPoint;
        bool relocate = false;
        if (approach && !AssassinApproach::Destination(context, target, medium_ == 1, approachPoint, relocate))
            return Empty(out);
        if (field && (!destination || context.selfIdentity.empty() || field_.Active()))
            return Empty(out);
        if (leap && (!destination || !DemonLeap::ValidPoint(context, *destination)))
            return Empty(out);
        const unsigned animation = engagement   ? 1730
                                   : jade       ? 2033
                                   : banish     ? 1350
                                   : assassin   ? 2270
                                   : leap       ? DemonLeap::Animation
                                   : backstep   ? AssassinBackstep::Animation
                                   : approach   ? AssassinApproach::Animation
                                   : field      ? InspiringReinforcement::Animation
                                   : absorption ? PainAbsorptionAction::Rules::AnimationMs(medium_ == 1)
                                   : rite       ? GreatDwarf::Animation
                                   : demon      ? (medium_ == 1 ? 2500 : 1667)
                                                : DwarfHealing::Animation;
        const unsigned activation = engagement   ? 1300
                                    : jade       ? 1200
                                    : banish     ? 620
                                    : assassin   ? 1700
                                    : leap       ? DemonLeap::Activation
                                    : backstep   ? AssassinBackstep::Activation
                                    : approach   ? AssassinApproach::Activation
                                    : field      ? InspiringReinforcement::Activation
                                    : absorption ? PainAbsorptionAction::Rules::ActivationMs(medium_ == 1)
                                    : rite       ? GreatDwarf::Activation
                                    : demon      ? DemonHealing::Activation
                                                 : DwarfHealing::Activation;
        const unsigned recharge = engagement           ? 3000
                                  : jade               ? 0
                                  : assassin           ? 30000
                                  : leap               ? DemonLeap::Recharge()
                                  : backstep           ? AssassinBackstep::Recharge()
                                  : approach           ? AssassinApproach::Recharge()
                                  : field              ? InspiringReinforcement::Recharge
                                  : rite || absorption ? 0
                                                       : DwarfHealing::Recharge;
        if (!context.wallNow || context.now > UINT64_MAX - animation - recharge ||
            context.wallNow > UINT64_MAX - animation - recharge)
            return WorldActionResult::Failed;
        if (rite && context.wallNow > UINT64_MAX - activation - GreatDwarf::Duration)
            return WorldActionResult::Failed;
        if (approach && (context.now > UINT64_MAX - activation - AssassinApproach::Duration ||
                         context.wallNow > UINT64_MAX - activation - AssassinApproach::Duration))
            return WorldActionResult::Failed;
        auto next = *this;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (breakTaunt && !next.plan_.empty()) {
            if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
            next.plan_.clear();
            next.melee_ = {};
            next.previous_.clear();
        }
        if (field &&
            !next.field_.Begin(context.position, *destination, context.now + activation, context.wallNow + activation))
            return Empty(out);
        if (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, changed, false))
            return WorldActionResult::Failed;
        if (!next.energy_.CanSpend(selected, context.now))
            return Empty(out);
        if (!next.energy_.Spend(selected, context.now, result))
            return WorldActionResult::Failed;
        std::vector<unsigned> phases;
        if (engagement)
            phases = {1730};
        else if (jade)
            phases = {2033};
        else if (banish)
            phases = {1350};
        else if (assassin)
            phases = {2270};
        else if (leap)
            phases = {80, 600, 1251};
        else if (backstep)
            phases = {500, 300, 200};
        else if (approach)
            phases = {367, 200, 766};
        else if (field)
            phases = {500, 733};
        else if (absorption)
            phases = {PainAbsorptionAction::Rules::AnimationMs(medium_ == 1)};
        else if (rite)
            phases = {2500, 1000};
        else if (demon)
            phases = medium_ == 1 ? std::vector<unsigned>{2500} : std::vector<unsigned>{667, 200, 100, 200, 500};
        else
            phases = {733, 1120};
        next.plan_.clear();
        next.supportPhaseEnds_.clear();
        uint64_t end = context.now;
        for (unsigned duration : phases) {
            july14812::SkillStage stage;
            stage.index = static_cast<uint8_t>(next.plan_.size());
            stage.durationMs = static_cast<uint16_t>(duration);
            if (approach && stage.index < 2) {
                stage.type = 1;
                stage.movement[3] = stage.movement[7] = 1;
                stage.movement[8] = stage.movement[9] = stage.index ? 1200.0f : 210.0f;
            }
            if (leap && stage.index == 1) {
                stage.type = 1;
                stage.movement[3] = stage.movement[7] = 1;
                stage.movement[8] = stage.movement[9] = 1000;
            }
            if (backstep && stage.index < 2) {
                stage.type = 1;
                stage.movement[2] = 1;
                stage.movement[3] = -4.371138828673793e-08f;
                stage.movement[7] = 1;
                stage.movement[8] = stage.movement[9] = 600;
            }
            next.plan_.push_back(stage);
            end += duration;
            next.supportPhaseEnds_.push_back(end);
        }
        next.skill_ = selected;
        next.slot_ = static_cast<uint8_t>(engagement   ? 2
                                          : jade       ? 4
                                          : banish     ? 2
                                          : leap       ? DemonLeap::Slot
                                          : backstep   ? AssassinBackstep::Slot
                                          : approach   ? AssassinApproach::Slot
                                          : field      ? InspiringReinforcement::Slot
                                          : absorption ? PainAbsorptionAction::Rules::Slot
                                          : rite       ? GreatDwarf::Slot
                                                       : DwarfHealing::Slot);
        next.identity_ = identity;
        next.target_ = (approach || banish || engagement) ? target : 0;
        next.ground_ = leap;
        if (leap)
            next.leapPoint_ = *destination;
        next.next_ = 0;
        next.recharge_ = recharge;
        next.healingFinishAt_ = context.now + animation;
        for (auto entry = next.deadlines_.begin(); entry != next.deadlines_.end();)
            if (entry->second <= context.wallNow)
                entry = next.deadlines_.erase(entry);
            else
                ++entry;
        if (recharge) {
            next.ready_[selected] = context.now + animation + recharge;
            next.deadlines_[selected] = context.wallNow + animation + recharge;
        }
        if (engagement) {
            WorldCombatTarget enemy;
            if (!context.combatTarget(target, enemy))
                return Empty(out);
            next.melee_ = {};
            next.melee_.skill = 26679;
            next.melee_.identity = identity;
            next.melee_.target = target;
            next.melee_.targetIdentity = enemy.actor.identity;
            next.melee_.maximum = 1200;
            next.melee_.medium = medium_;
            next.melee_.settlesAt = context.now + 1300;
            next.melee_.impactUtc = context.wallNow + 1300;
        } else if (jade) {
            next.melee_ = {};
            next.melee_.skill = 28406;
            next.melee_.identity = identity;
            next.melee_.medium = medium_;
            next.melee_.settlesAt = context.now + 1200;
            next.melee_.impactUtc = context.wallNow + 1200;
        } else if (banish) {
            WorldCombatTarget enemy;
            if (!context.combatTarget(target, enemy))
                return Empty(out);
            next.melee_ = {};
            next.melee_.skill = 27505;
            next.melee_.identity = identity;
            next.melee_.target = target;
            next.melee_.targetIdentity = enemy.actor.identity;
            next.melee_.maximum = 600;
            next.melee_.medium = medium_;
            next.melee_.settlesAt = context.now + 620;
            next.melee_.impactUtc = context.wallNow + 620;
        } else if (leap && context.combatEnemies && context.persistCombatHits) {
            next.melee_ = {};
            next.melee_.skill = 27917;
            next.melee_.identity = identity;
            next.melee_.medium = medium_;
            next.melee_.settlesAt = context.now + activation;
            next.melee_.impactUtc = context.wallNow + activation;
        } else if (leap) {
        } // Movement-only component context has no hostile contract.
        else if (backstep) {
            next.evasion_.Begin(context.now);
            next.backstepDue_ = context.now + activation;
            next.backstepUtc_ = context.wallNow + activation;
        } else if (approach)
            next.approachDue_ = context.now + activation;
        else if (field) {
        } // Connection-local field was scheduled above; no immediate grant.
        else if (absorption) {
            if (!next.absorption_.Schedule(context, medium_ == 1))
                return WorldActionResult::Failed;
        } else if (rite) {
            next.riteDue_ = context.now + activation;
            next.riteUtc_ = context.wallNow + activation;
        } else {
            modern::PendingHeal heal;
            heal.startsAt = context.now + activation;
            heal.skill = selected;
            heal.amount = assassin ? 0 : demon ? DemonHealing::Base(level_) : DwarfHealing::Amount(level_);
            heal.perCondition = demon ? DemonHealing::PerCondition(level_) : 0;
            next.pendingHeals_.push_back(heal);
        }
        Buffer echo;
        WorldActionOutput phase;
        if (OwnMotion(input, agent_, context.sequence, echo) != RelayResult::Encoded ||
            !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()) ||
            !next.Emit(context, context.sequence + 1, phase) || !result.owned.Append(phase.owned.View()) ||
            !result.peers.Append(phase.peers.View()))
            return WorldActionResult::Failed;
        result.sequences = 2;
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (backstep && (context.stunned || context.taunted)) {
            if (!context.persistStunBreakState ||
                !context.persistStunBreakState(Text(next.deadlines_),
                                               next.hammers_.Text(next.EffectText(), next.energy_, context)))
                return Empty(out);
        } else if (!next.SaveEffects(context))
            return Empty(out);
        next.lastIdentity_ = identity;
        next.started_ = true;
        result.peerEffectVersion = ++next.version_;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult FacetRequest(ByteView input, const WorldActionContext& context, uint32_t identity, unsigned slot,
                                   WorldActionOutput& out) {
        static const unsigned bases[] = {27220, 28379, 26644, 27014, 27760};
        float health = 0;
        if (slot >= 5 || !modern::ReadHealthState(context.healthState, health, 100))
            return WorldActionResult::Failed;
        if (medium_ == 1 || legends_.First() != 11 || !level_ || level_ > 80 || health == 0 || !plan_.empty() ||
            !pendingHeals_.empty() || riteDue_ || approachDue_ || backstepDue_ || absorption_.Pending() ||
            version_ >= UINT64_MAX - 1)
            return Empty(out);
        const auto base = bases[slot];
        const auto rule = modern::FacetData<49284>::RuleFor(base);
        const bool consume = hammers_.FacetActive(base), release = consume && base == 27760;
        const bool healing = consume && base == 27220, strength = consume && base == 26644;
        const bool darkness = consume && base == 28379, elements = consume && base == 27014;
        if ((hammers_.HasFacet(base) && !consume) ||
            (consume && !healing && !release && !strength && !darkness && !elements) || release_.Pending())
            return Empty(out);
        const auto selected = consume ? rule.alternate : base;
        const auto action = modern::FacetData<49284>::RuleFor(selected);
        // The existing alternate recharge gates the base; do not invent a second cooldown.
        const auto ready = ready_.find(rule.alternate);
        if (ready != ready_.end() && context.now < ready->second)
            return Empty(out);
        if (!context.wallNow || context.now > UINT64_MAX - action.animation - action.recharge ||
            context.wallNow > UINT64_MAX - action.animation - action.recharge)
            return WorldActionResult::Failed;
        auto next = *this;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, changed, false))
            return WorldActionResult::Failed;
        std::string healthText = context.healthState;
        if (consume) {
            if (!next.hammers_.ConsumeFacet(base, context, next.energy_, result))
                return Empty(out);
            if (release) {
                if (!next.release_.Schedule(context, false))
                    return WorldActionResult::Failed;
            } else if (healing) {
                Buffer conversion;
                if (!next.healingEffects_.Start(agent_, context, context.now, level_, conversion,
                                                combat::InfuseLight::Skill) ||
                    !result.owned.Append(conversion.View()) || !result.peers.Append(conversion.View()))
                    return WorldActionResult::Failed;
                const float amount = static_cast<float>(125 + static_cast<unsigned>(level_ * level_ * 0.27f + 0.5f));
                const float current = (std::min)(100.0f, health + amount);
                Buffer delta;
                if (!modern::WriteHealthState(current, healthText, 100) ||
                    (current > health && !modern::HealthChange<49284>(agent_, 27228, current - health, delta)) ||
                    !result.owned.Append(delta.View()) || !result.peers.Append(delta.View()))
                    return WorldActionResult::Failed;
            }
            // The instantaneous heal publishes now. Animated consumption uses
            // the existing end-of-animation cooldown and persisted deadline.
            if ((healing || darkness) &&
                !SkillbarRecharge(agent_, selected, action.recharge, action.recharge, result.owned))
                return WorldActionResult::Failed;
            next.ready_[selected] = context.now + action.animation + action.recharge;
            next.deadlines_[selected] = context.wallNow + action.animation + action.recharge;
        } else {
            uint32_t instance = 0;
            if (!next.energy_.Spend(base, context.now, result) || !next.healingEffects_.Reserve(instance) ||
                !next.hammers_.ScheduleFacet(base, instance, context, next.energy_, false))
                return Empty(out);
        }
        Buffer echo;
        if (OwnMotion(input, agent_, context.sequence, echo) != RelayResult::Encoded ||
            !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()))
            return WorldActionResult::Failed;
        result.sequences = 1;
        next.skill_ = selected;
        next.slot_ = static_cast<uint8_t>(slot);
        next.identity_ = identity;
        if (action.animation) {
            july14812::SkillStage stage;
            stage.durationMs = static_cast<uint16_t>(action.animation);
            next.plan_.assign(1, stage);
            next.supportPhaseEnds_.assign(1, context.now + action.animation);
            next.healingFinishAt_ = context.now + action.animation;
            next.target_ = 0;
            next.ground_ = false;
            next.next_ = 0;
            next.recharge_ = action.recharge;
            WorldActionOutput phase;
            if (!next.Emit(context, context.sequence + 1, phase) || !result.owned.Append(phase.owned.View()) ||
                !result.peers.Append(phase.peers.View()))
                return WorldActionResult::Failed;
            ++result.sequences;
        }
        if (elements || strength) {
            next.melee_ = {};
            next.melee_.skill = selected;
            next.melee_.identity = identity;
            next.melee_.medium = medium_;
            next.melee_.settlesAt = context.now + action.activation;
            next.melee_.impactUtc = context.wallNow + action.activation;
        }
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        const auto hostile =
            darkness ? combat::GazeDarkness::Commit(49284, context, Text(next.deadlines_),
                                                    next.hammers_.Text(next.EffectText(), next.energy_, context))
                     : WorldConditionCommit::Applied;
        if (hostile == WorldConditionCommit::RecoveryRequired)
            return WorldActionResult::Failed;
        const bool saved =
            darkness  ? hostile == WorldConditionCommit::Applied
            : healing ? (context.persistCombatConditions &&
                         context.persistCombatConditions(Text(next.deadlines_),
                                                         next.hammers_.Text(next.EffectText(), next.energy_, context),
                                                         healthText, context.conditionState))
                      : next.SaveEffects(context);
        if (!saved)
            return Empty(out);
        next.lastIdentity_ = identity;
        next.started_ = true;
        result.peerEffectVersion = ++next.version_;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult HammerRequest(ByteView input, const WorldActionContext& context, uint32_t identity,
                                    WorldActionOutput& out, unsigned selected = Hammers::Skill) {
        float health = 0;
        if (context.sequence > UINT32_MAX - 3 || !modern::ReadHealthState(context.healthState, health, 100))
            return WorldActionResult::Failed;
        if (legends_.First() != Hammers::LegendFor(selected) || !level_ || level_ > 80 || health == 0)
            return Empty(out);
        if (hammers_.Present() && hammers_.CurrentSkill() != selected)
            return Empty(out);
        const bool off = hammers_.Active(context.now);
        if (off && !plan_.empty() && (!Hammers::IsMaintainedSkill(skill_) || Hammers::StartFor(skill_) != selected))
            return Empty(out);
        const unsigned action = off ? Hammers::StopFor(selected) : selected;
        if ((hammers_.Present() && !off) || (!off && !plan_.empty()) || version_ >= UINT64_MAX - 1)
            return Empty(out);
        if (!off && !energy_.CanSpend(selected, context.now))
            return Empty(out);
        auto next = *this;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, changed, false))
            return WorldActionResult::Failed;
        if (off) {
            if (!next.hammers_.Stop(context, next.energy_, result))
                return WorldActionResult::Failed;
            if (!next.plan_.empty() && Hammers::IsMaintainedSkill(next.skill_)) {
                if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                    !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                    return WorldActionResult::Failed;
                ++result.sequences;
                next.plan_.clear();
            }
        } else {
            uint32_t instance = 0, auxiliary1 = 0, auxiliary2 = 0;
            if (!next.energy_.Spend(selected, context.now, result) || !next.healingEffects_.Reserve(instance) ||
                (Hammers::IsAssassinSkill(selected) &&
                 (!next.healingEffects_.Reserve(auxiliary1) || !next.healingEffects_.Reserve(auxiliary2))) ||
                !next.hammers_.Schedule(instance, context, next.energy_, selected, medium_ == 1, auxiliary1,
                                        auxiliary2))
                return WorldActionResult::Failed;
            if (Hammers::IsAssassinSkill(selected) &&
                (!next.hammers_.Advance(context, next.legends_.First(), level_, next.energy_, result, changed) ||
                 !next.energy_.Advance(context.now, result)))
                return WorldActionResult::Failed;
        }
        Buffer echo;
        if (OwnMotion(input, agent_, context.sequence + result.sequences, echo) != RelayResult::Encoded ||
            !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()))
            return WorldActionResult::Failed;
        ++result.sequences;
        if (Hammers::IsAssassinSkill(selected)) {
            // Both own tokens are zero. Echo the native order without creating
            // a zero-duration phase or importing another action's animation.
            next.skill_ = action;
            next.slot_ = 2;
            next.identity_ = identity;
        } else if (!off || Hammers::IsEliteSkill(selected)) {
            july14812::SkillStage stage;
            stage.durationMs = static_cast<uint16_t>(Hammers::AnimationFor(action, medium_ == 1));
            next.plan_.assign(1, stage);
            next.skill_ = action;
            next.slot_ = static_cast<uint8_t>(Hammers::IsEliteSkill(selected) ? DemonElite::Slot : Hammers::Slot);
            next.identity_ = identity;
            next.target_ = 0;
            next.ground_ = false;
            next.next_ = 0;
            next.recharge_ = 0;
            WorldActionOutput phase;
            if (!next.Emit(context, context.sequence + result.sequences, phase) ||
                !result.owned.Append(phase.owned.View()) || !result.peers.Append(phase.peers.View()))
                return WorldActionResult::Failed;
            ++result.sequences;
        }
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!next.SaveEffects(context))
            return Empty(out);
        next.lastIdentity_ = identity;
        next.started_ = true;
        result.peerEffectVersion = ++next.version_;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult ChangeLegend(ByteView input, const WorldActionContext& context, bool swap, unsigned slot,
                                   unsigned legend, uint32_t identity, WorldActionOutput& out) {
        if (recipe_.profession != 9)
            return Empty(out);
        if (context.peerAgent != agent_ || !context.sequence || context.sequence >= UINT32_MAX - 1 ||
            version_ == UINT64_MAX)
            return WorldActionResult::Failed;
        if (swap && identity == lastIdentity_)
            return Empty(out);
        if (!Restore(context))
            return WorldActionResult::Failed;
        auto next = *this;
        if (!next.legends_.Restore(context.now, context.wallNow))
            return WorldActionResult::Failed;
        const auto previous = next.legends_.Text();
        const auto active = next.legends_.First();
        if (swap ? !next.legends_.Swap(context.now, context.wallNow)
                 : !next.legends_.Select(slot, legend, level_, context.now))
            return Empty(out);
        if (next.legends_.Text() == previous)
            return Empty(out);
        WorldActionOutput result;
        result.sequences = swap ? 1 : 0;
        if (!LegendSelection(agent_, next.legends_, next.legends_.Remaining(context.now), result.owned) ||
            !LegendSelection(agent_, next.legends_, 0, result.peers))
            return WorldActionResult::Failed;
        if (next.legends_.First() != active) {
            next.field_.Cancel();
            next.pendingHeals_.clear();
            next.riteDue_ = next.riteUtc_ = 0;
            next.approachDue_ = 0;
            next.backstepDue_ = 0;
            next.absorption_.Cancel();
            next.release_.Cancel();
            if (!next.hammers_.Stop(context, next.energy_, result) || !next.energy_.Reset(context.now, result))
                return WorldActionResult::Failed;
            if ((Hammers::IsMaintainedSkill(next.skill_) || IsSupportAction(next.skill_)) && !next.plan_.empty()) {
                if (!SkillEnd(agent_, context.sequence, next.identity_, result.owned) ||
                    !SkillEnd(agent_, context.sequence, next.identity_, result.peers))
                    return WorldActionResult::Failed;
                ++result.sequences;
                next.plan_.clear();
            }
        }
        if (swap) {
            Buffer echo;
            if (OwnMotion(input, agent_, context.sequence + result.sequences - 1, echo) != RelayResult::Encoded ||
                !result.owned.Append(echo.View()) || !result.peers.Append(echo.View()))
                return WorldActionResult::Failed;
        }
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        const bool saved =
            next.hammers_.Tracked()
                ? (context.persistLegendSkillState &&
                   context.persistLegendSkillState(next.legends_.Text(), Text(next.deadlines_),
                                                   next.hammers_.Text(next.EffectText(), next.energy_, context)))
                : (context.persistLegendState && context.persistLegendState(next.legends_.Text()));
        if (!saved)
            return Empty(out);
        next.legendsReported_ = true;
        ++next.version_;
        if (swap)
            next.lastIdentity_ = identity;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool VenomousSphereAvailable() const {
        if (recipe_.profession != 9 || medium_ != 1)
            return false;
        const auto* selected = SelectStarterSkill(recipe_, equipped_, level_, 24, 7, attunement_, 0, 0);
        return selected && selected->skill == VenomousSphereRules::Base;
    }
    bool BrandAvailable() const {
        if (recipe_.profession != 9 || medium_ != 1)
            return false;
        const auto* selected = SelectStarterSkill(recipe_, equipped_, level_, 24, 8, attunement_, 0, 0);
        return selected && selected->skill == BrandRules::Base;
    }
    bool EngineerAvailable(const EngineerProjectileRule& rule) const {
        if (recipe_.profession != 3 || medium_ != 1)
            return false;
        const auto* selected =
            SelectStarterSkill(recipe_, equipped_, level_, 24, static_cast<uint8_t>(rule.slot), attunement_, 0, 0);
        return selected && selected->skill == rule.skill;
    }
    bool MagneticAvailable() const {
        if (recipe_.profession != 6 || medium_ != 0 || attunement_ != 4)
            return false;
        const auto* selected = SelectStarterSkill(recipe_, equipped_, level_, 29, 7, 4, 0, 0);
        return selected && selected->skill == kMagneticBase;
    }
    bool BubbleAvailable(const BubbleRule& rule) const {
        if (recipe_.profession != 6 || medium_ != 1 || attunement_ != rule.attunement)
            return false;
        const auto* selected =
            SelectStarterSkill(recipe_, equipped_, level_, 24, static_cast<uint8_t>(rule.slot), attunement_, 0, 0);
        return selected && selected->skill == rule.skill;
    }
    bool ActionInRange(const WorldActionContext& context) const {
        if (skill_ == DemonLeap::Skill)
            return DemonLeap::ValidPoint(context, leapPoint_);
        if (skill_ == AssassinApproach::Skill) {
            CatalogPosition destination;
            bool relocate = false;
            return AssassinApproach::Destination(context, target_, medium_ == 1, destination, relocate);
        }
        if (!ground_)
            return TargetInRange(target_, skill_, context);
        const auto* definition = StarterDefinitionFor(skill_);
        std::array<float, 3> origin;
        if (!definition || !NativePosition(context.position.x, context.position.y, context.position.z, origin))
            return false;
        double squared = 0;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(groundPoint_[i]))
                return false;
            const double delta = double(groundPoint_[i]) - origin[i];
            squared += delta * delta;
        }
        return std::isfinite(squared) && squared <= double(definition->maximumRange) * definition->maximumRange;
    }
    bool WeaponSelection(Buffer& out) const {
        return out.U16(218) && out.VarU32(agent_) && out.U8(medium_ == 1 ? 0 : 4);
    }
    bool TargetAvailable(uint32_t target, const WorldActionContext& context) const {
        if (!target || target == agent_)
            return true;
        CatalogPosition point;
        return context.isVisibleCharacterTarget && context.isVisibleCharacterTarget(target) &&
               context.characterTargetPosition && context.characterTargetPosition(target, point) &&
               std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
    bool TargetInRange(uint32_t target, uint32_t skill, const WorldActionContext& context) const {
        if (!target || target == agent_)
            return true;
        if (!TargetAvailable(target, context))
            return false;
        const auto* definition = StarterDefinitionFor(skill);
        CatalogPosition point;
        if (!definition || !context.characterTargetPosition(target, point))
            return false;
        // Convert world positions to the authored distance scale (32:1).
        // Radius adjustments require collision geometry, so use centre distance.
        const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                     z = double(point.z) - context.position.z;
        const double squared = (x * x + y * y + z * z) * 1024;
        return std::isfinite(squared) && squared <= double(definition->maximumRange) * definition->maximumRange;
    }
    static WorldActionResult Empty(WorldActionOutput& out) {
        WorldActionOutput empty;
        empty.sequences = 0;
        out = std::move(empty);
        return WorldActionResult::Handled;
    }
    static bool Decimal(const std::string& text, uint64_t& out) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        uint64_t value = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || value > (UINT64_MAX - (c - '0')) / 10)
                return false;
            value = value * 10 + (c - '0');
        }
        out = value;
        return true;
    }
    static std::string Text(const std::map<uint32_t, uint64_t>& values) {
        std::string text;
        for (const auto& entry : values) {
            if (!text.empty())
                text += ',';
            text += std::to_string(entry.first) + ':' + std::to_string(entry.second);
        }
        return text;
    }
    static bool SkillbarRecharge(uint32_t actor, uint32_t skill, uint32_t remaining, uint32_t total, Buffer& out) {
        Buffer bytes;
        if (!hot49284::SkillRecharge(actor, skill, remaining, total, bytes))
            return false;
        // Consumption removes the marker that selected the alternate. Publish
        // its existing deadline on the base now displayed by this same slot.
        // Persistence and admission continue to use only the alternate key.
        const auto base = modern::FacetData<49284>::BaseFor(skill);
        if (base && base != skill && !hot49284::SkillRecharge(actor, base, remaining, total, bytes))
            return false;
        return out.Append(bytes.View());
    }
    uint32_t Recharge(uint32_t skill) const {
        if (recipe_.profession == 9 && skill == 26679)
            return 3000;
        if (recipe_.profession == 9 && skill == 26937)
            return 30000;
        if (recipe_.profession == 5 && skill == ThiefHealing::Skill)
            return ThiefHealing::Recharge;
        if (recipe_.profession == 5 && skill == SignetMaliceHealing::Skill)
            return SignetMaliceHealing::Recharge;
        if (recipe_.profession == 3 && skill == EngineerHealing::Skill)
            return EngineerHealing::Recharge;
        if (recipe_.profession == 6 && skill == ElementalistHealing::Skill)
            return ElementalistHealing::Recharge;
        if (recipe_.profession == 9 &&
            (skill == 27228 || skill == 27080 || skill == 27162 || skill == 28075 || skill == 28113))
            return modern::FacetData<49284>::RuleFor(skill).recharge;
        if (recipe_.profession == 9 && skill == InspiringReinforcement::Skill)
            return InspiringReinforcement::Recharge;
        if (recipe_.profession == 9 && (skill == DwarfHealing::Skill || skill == DemonHealing::Skill))
            return DwarfHealing::Recharge;
        bool own = false;
        for (const auto& choice : StarterSkillsData().choices)
            if (choice.profession == recipe_.profession && choice.skill == skill) {
                own = true;
                break;
            }
        const auto* definition = own ? StarterDefinitionFor(skill) : nullptr;
        return definition && definition->recharge > 0 ? static_cast<uint32_t>(definition->recharge) : 0;
    }
    bool Restore(const WorldActionContext& context) {
        if (restored_)
            return hammers_.ClockValid(context);
        std::map<uint32_t, uint64_t> ready;
        std::map<uint32_t, uint32_t> resume;
        for (const auto& entry : deadlines_) {
            if (!context.wallNow)
                return false;
            const auto remaining = entry.second > context.wallNow ? entry.second - context.wallNow : 0;
            const auto total = Recharge(entry.first);
            if (!total || remaining > uint64_t(total) + 60000 || context.now > UINT64_MAX - remaining)
                return false;
            if (remaining)
                ready[entry.first] = context.now + remaining;
            resume[entry.first] = total;
        }
        auto aura = aura_;
        auto healingEffects = healingEffects_;
        auto hammers = hammers_;
        auto energy = energy_;
        if (!aura.Restore(context.now, context.wallNow) || !healingEffects.Restore(context.now, context.wallNow) ||
            !hammers.Restore(context, energy) || !hammers.ClockValid(context))
            return false;
        if (hammers.NeedsSave() || healingEffects.Text() != healingEffects_.Text() ||
            aura.ReceivedText() != aura_.ReceivedText()) {
            if (!context.persistSkillState ||
                !context.persistSkillState(
                    Text(deadlines_),
                    hammers.Text(recipe_.profession == 9 ? healingEffects.Text() : aura.Text(), energy, context)))
                return false;
            hammers.Saved();
        }
        aura_ = std::move(aura);
        healingEffects_ = std::move(healingEffects);
        hammers_ = std::move(hammers);
        energy_ = std::move(energy);
        ready_ = std::move(ready);
        resume_ = std::move(resume);
        restored_ = true;
        return true;
    }
    bool Emit(const WorldActionContext& context, uint32_t sequence, WorldActionOutput& out) {
        if (next_ >= plan_.size() || context.now > UINT64_MAX - plan_[next_].durationMs)
            return false;
        const auto& stage = plan_[next_];
        july14812::SkillMovement move;
        bool moving = stage.type != 0;
        std::array<float, 3> facing = context.hasFacing
                                          ? std::array<float, 3>{{context.facing.x, context.facing.y, context.facing.z}}
                                          : std::array<float, 3>{{1, 0, 0}};
        double distance = std::numeric_limits<double>::infinity();
        if (target_ && target_ != agent_) {
            CatalogPosition point;
            if (!context.characterTargetPosition || !context.characterTargetPosition(target_, point))
                return false;
            if (skill_ == AssassinApproach::Skill) {
                bool relocate = false;
                if (!AssassinApproach::Destination(context, target_, medium_ == 1, point, relocate))
                    return false;
                if (!relocate)
                    moving = false;
            }
            const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                         z = medium_ == 1 ? double(point.z) - context.position.z : 0;
            distance = std::sqrt(x * x + y * y + z * z) * 32;
            if (!std::isfinite(distance))
                return false;
            if (distance > 0.001)
                facing = {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}};
        }
        if (skill_ == kMagneticFollowup) {
            if (!target_ || target_ == agent_ || !std::isfinite(distance))
                return false;
            const double length = std::hypot(facing[0], facing[1]);
            moving = distance >= 1 && length > .001;
            if (moving) {
                move.direction =
                    move.facing = {{static_cast<float>(facing[0] / length), static_cast<float>(facing[1] / length), 0}};
                move.speed = static_cast<float>(distance * 1000 / stage.durationMs);
                moving = move.speed >= 1;
            }
        } else if (moving && skill_ == DemonLeap::Skill) {
            if (!DemonLeap::Aim(context, leapPoint_, medium_ == 1, stage.durationMs, move.direction, move.speed,
                                moving))
                return false;
            move.facing = move.direction;
        } else if (moving) {
            if (!july14812::BuildSkillMovement(stage, facing, move, medium_ == 1))
                return false;
            if (move.direction[0] * facing[0] + move.direction[1] * facing[1] + move.direction[2] * facing[2] > 0)
                move.speed = (std::min)(move.speed, static_cast<float>(distance * 1000 / stage.durationMs));
            if (move.speed < 1)
                moving = false;
        }
        if (!started_ && version_ == UINT64_MAX)
            return false;
        if (!SkillPhase(agent_, sequence, identity_, skill_, slot_, target_, stage, next_ == 0,
                        next_ + 1 == plan_.size(), moving ? &move : nullptr, medium_ == 1, out.owned) ||
            !out.peers.Append(out.owned.View()))
            return false;
        emittedMovement_ = move;
        emittedMoving_ = moving;
        if (!started_)
            ++version_;
        started_ = true;
        due_ = context.now + stage.durationMs;
        ++next_;
        return true;
    }
    bool MeleeDue(uint64_t now) const {
        if (melee_.skill == 26679)
            return plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.First() != 13 || now >= melee_.settlesAt;
        if (melee_.skill == 27917)
            return plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.First() != 14 || now >= melee_.settlesAt;
        if (melee_.skill == 27162 || melee_.skill == 28113)
            return plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.First() != 11 || now >= melee_.settlesAt;
        if (melee_.skill == 28406)
            return plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.First() != 12 || now >= melee_.settlesAt;
        if (melee_.skill == 27505)
            return plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.First() != 14 || now >= melee_.settlesAt;
        return melee_.skill &&
               (plan_.empty() || skill_ != melee_.skill || identity_ != melee_.identity || medium_ != melee_.medium ||
                !EquippedStarterWeapon(recipe_, equipped_, medium_ == 1 ? 24 : 29) ||
                (next_ >= plan_.size() && now >= due_));
    }
    WorldActionResult FinishMelee(const WorldActionContext& c, WorldActionOutput& out) {
        if (c.peerAgent != agent_ || !c.sequence)
            return WorldActionResult::Failed;
        auto next = *this;
        if (melee_.skill == 26679) {
            if (plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium || legends_.First() != 13) {
                next.melee_ = {};
                out = {};
                out.sequences = 0;
            } else {
                const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
                if (hit < 0) {
                    combatRecovery_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    return WorldActionResult::Handled;
            }
            *this = std::move(next);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27917) {
            if (plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium || legends_.First() != 14) {
                next.melee_ = {};
                out = {};
                out.sequences = 0;
            } else {
                const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
                if (hit < 0) {
                    combatRecovery_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    return WorldActionResult::Handled;
            }
            *this = std::move(next);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27162 || melee_.skill == 28113) {
            if (plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium || legends_.First() != 11) {
                next.melee_ = {};
                out = {};
                out.sequences = 0;
            } else {
                const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
                if (hit < 0) {
                    combatRecovery_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    return WorldActionResult::Handled;
            }
            *this = std::move(next);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 28406) {
            if (plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium || legends_.First() != 12) {
                next.melee_ = {};
                out = {};
                out.sequences = 0;
            } else {
                const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
                if (hit < 0) {
                    combatRecovery_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    return WorldActionResult::Handled;
            }
            *this = std::move(next);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27505) {
            if (plan_.empty() || identity_ != melee_.identity || medium_ != melee_.medium || legends_.First() != 14) {
                next.melee_ = {};
                out = {};
                out.sequences = 0;
            } else {
                const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
                if (hit < 0) {
                    combatRecovery_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    return WorldActionResult::Handled;
            }
            *this = std::move(next);
            return WorldActionResult::Handled;
        }
        if (plan_.empty() || skill_ != melee_.skill || identity_ != melee_.identity || medium_ != melee_.medium ||
            !EquippedStarterWeapon(recipe_, equipped_, medium_ == 1 ? 24 : 29)) {
            next.melee_ = {};
            out = {};
            out.sequences = 0;
        } else {
            if (c.now > UINT64_MAX - 6000)
                return WorldActionResult::Failed;
            next.previous_[slot_] = skill_;
            next.chainUntil_ = c.now + 6000;
            next.plan_.clear();
            const int hit = next.melee_.Settle(49284, agent_, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (!hit)
                return WorldActionResult::Handled;
        }
        *this = std::move(next);
        return WorldActionResult::Handled;
    }
    uint16_t agent_;
    Hot49284Loadout recipe_;
    unsigned level_;
    uint8_t equipped_, medium_ = 0, attunement_ = 1;
    bool mediumReported_ = false, restored_ = false, attunementReported_ = false;
    beta::MeleeContact melee_;
    bool combatRecovery_ = false;
    uint64_t version_ = 0;
    SelfEffect aura_;
    DwarfHealingEffects healingEffects_;
    modern::ConditionActions<49284> conditions_;
    std::vector<modern::PendingHeal> pendingHeals_;
    uint64_t personalHealUtc_ = 0, stealthAttackDue_ = 0;
    PainAbsorptionAction absorption_;
    modern::DragonRelease<49284> release_;
    modern::InspiringField<49284> field_;
    uint64_t healingFinishAt_ = 0, riteDue_ = 0, riteUtc_ = 0, approachDue_ = 0, backstepDue_ = 0, backstepUtc_ = 0;
    std::vector<uint64_t> supportPhaseEnds_;
    modern::Energy<49284> energy_;
    Hammers hammers_;
    LegendState legends_;
    bool legendsReported_ = false;
    bool initiativeAnnounced_ = false;
    bool enduranceAnnounced_ = false;
    unsigned endurancePoints_ = 100;
    uint32_t lastDodgeToken_ = 0;
    combat::DodgeEvasion evasion_;
    modern::StarterMissiles missiles_;
    MagneticGrasp grasp_;
    TargetLink<BrandRules> brand_;
    uint64_t warpDue_ = 0;
    bool warpPending_ = false;
    uint32_t lastIdentity_ = 0, skill_ = 0, identity_ = 0, target_ = 0, recharge_ = 0;
    uint8_t slot_ = 0;
    size_t next_ = 0;
    uint64_t due_ = 0, chainUntil_ = 0;
    july14812::SkillMovement emittedMovement_;
    bool emittedMoving_ = false, started_ = false;
    CatalogPosition leapPoint_;
    std::array<float, 3> groundPoint_{};
    bool ground_ = false;
    std::vector<july14812::SkillStage> plan_;
    std::map<unsigned, uint32_t> previous_;
    std::map<uint32_t, uint64_t> deadlines_, ready_;
    std::map<uint32_t, uint32_t> resume_;
};
} // namespace hot49284
} // namespace glide
