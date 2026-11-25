#include "glide/profile/demon_leap_impact.h"
#include "glide/profile/elemental_blast.h"
#include "glide/profile/gaze_darkness.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/boon_removal.h"
#include "glide/profile/modern_brand.h"
#include "glide/profile/dodge_evasion.h"
#include "glide/profile/modern_stealth.h"
#include "glide/profile/modern_energy.h"
#include "glide/profile/modern_health.h"
#include "glide/profile/beta_melee_combat.h"
#include "glide/profile/signet_malice_healing.h"
#include "glide/profile/modern_conditions.h"
#include "glide/profile/pain_absorption.h"
#include "glide/profile/inspiring_field.h"
#include "glide/profile/dragon_release.h"
#include "glide/profile/great_dwarf_allies.h"
#include "glide/profile/target_approach.h"
#include "glide/profile/backstep_data.h"
#include "glide/profile/endurance.h"
#include "glide/profile/initiative.h"
#include "glide/profile/demon_leap_data.h"
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/china_dwarf_healing_data.h"
#include "glide/profile/modern_legends.h"
#include "glide/profile/modern_hammers.h"
#include "glide/profile/modern_revenant_sphere.h"
#include "../../204489/server/aquatic_skills.h"
#include "inventory_state.h"
#include "profession_state.h"
#include "self_effect.h"
#include "glide/profile/modern_missile.h"
#include "starter_item_binding.h"
#include "../../204489/server/aquatic_equipment.h"
#include "game.h"
#include "bootstrap.h"
#include <utility>
#include <cmath>
#include <cstring>
#include <set>

namespace glide {
namespace china167409 {
namespace {
using DemonHealingData = modern::DemonHealingData<167409>;
using Maintained = modern::Hammers<167409>;
using Absorption = modern::PainAbsorption<167409>;
using Inspiring = modern::InspiringField<167409>;
using Approach = modern::TargetApproach<167409>;
using Backstep = modern::BackstepData<167409>;
using DemonLeap = modern::DemonLeapData<167409>;
using Facets = modern::FacetData<167409>;
using DragonRelease = modern::DragonRelease<167409>;
using RiteAllies = modern::GreatDwarfAllies<167409>;
void ConfigureAssassinInitialHeal(InitialWeaponSkill& skill, bool water) {
    // Skill 26937 initial model/event, associated by coefficient0.25; the
    // attack-triggered768 model is handled by the shared successful-hit transaction.
    skill.activationMs = 450;
    skill.animationMs = water ? 1667 : 1220;
    skill.recoveryMs = skill.animationMs - skill.activationMs;
    skill.rechargeMs = 30000;
    skill.variableStages = false;
    skill.phases.clear();
    skill.underwater = false;
    skill.groundTargeted = false;
    skill.minimumGroundRange = skill.maximumGroundRange = 0;
}
void ConfigureDemonLeap(InitialWeaponSkill& skill, bool water) {
    skill.activationMs = DemonLeap::Activation;
    skill.animationMs = DemonLeap::Animation;
    skill.recoveryMs = DemonLeap::Animation - DemonLeap::Activation;
    skill.rechargeMs = DemonLeap::Recharge();
    skill.variableStages = true;
    skill.underwater = water;
    skill.groundTargeted = true;
    skill.minimumGroundRange = 0;
    skill.maximumGroundRange = DemonLeap::Range;
    skill.phases.clear();
    for (unsigned i = 0; i < 3; ++i) {
        modern::SkillStage stage;
        stage.index = static_cast<uint8_t>(i);
        stage.type = i == 1 ? 1 : 0;
        stage.durationMs = static_cast<uint16_t>(i == 0 ? 80 : i == 1 ? 600 : 1251);
        if (stage.type)
            stage.movement = {{0, 0, 0, 1, 0, 0, 0, 1, 1000, 1000}};
        skill.phases.push_back(stage);
    }
}
void ConfigureBackstep(InitialWeaponSkill& skill, bool water) {
    skill.activationMs = Backstep::Activation;
    skill.animationMs = Backstep::Animation;
    skill.recoveryMs = Backstep::Animation - Backstep::Activation;
    skill.rechargeMs = 0;
    skill.variableStages = true;
    skill.underwater = water;
    skill.groundTargeted = false;
    skill.minimumGroundRange = skill.maximumGroundRange = 0;
    skill.phases.clear();
    for (unsigned i = 0; i < (water ? 2u : 3u); ++i) {
        modern::SkillStage stage;
        stage.index = static_cast<uint8_t>(i);
        stage.type = i < (water ? 1u : 2u) ? 1 : 0;
        stage.durationMs = static_cast<uint16_t>(water ? (i ? 200 : 800) : (i == 0 ? 500 : i == 1 ? 300 : 200));
        if (stage.type)
            stage.movement = {{0, 0, 1, -4.371138828673793e-8f, 0, 0, 0, 1, 600, 600}};
        skill.phases.push_back(stage);
    }
}
void ConfigureApproach(InitialWeaponSkill& skill, bool water) {
    // Own phases in all three archives; trigger0 is569, not the aggregate row369.
    skill.activationMs = Approach::Activation;
    skill.animationMs = Approach::Animation;
    skill.recoveryMs = Approach::Animation - Approach::Activation;
    skill.rechargeMs = Approach::Recharge();
    skill.variableStages = true;
    skill.underwater = water;
    skill.groundTargeted = false;
    skill.minimumGroundRange = 0;
    skill.maximumGroundRange = Approach::Range;
    skill.phases.clear();
    for (unsigned i = 0; i < 3; ++i) {
        modern::SkillStage stage;
        stage.index = static_cast<uint8_t>(i);
        stage.type = i < 2 ? 1 : 0;
        stage.durationMs = static_cast<uint16_t>(i == 0 ? 367 : i == 1 ? 200 : 766);
        if (i < 2)
            stage.movement = {{0, 0, 0, 1, 0, 0, 0, 1, i == 0 ? 210.0f : 1200.0f, i == 0 ? 210.0f : 1200.0f}};
        skill.phases.push_back(stage);
    }
}
bool MoveReply(uint16_t agent, uint32_t instance, uint32_t slot, Buffer& out) {
    // Original S274 moves an existing item and swaps occupied cells without deleting it.
    return out.U16(274) && out.VarU32(agent) && out.VarU32(instance) && out.U8(3) && out.VarU32(slot);
}
struct InventoryReports {
    std::array<Buffer, 68> unbound, bound;
    const Buffer& Get(size_t item, bool isBound) const {
        return isBound ? bound[item] : unbound[item];
    }
};
bool EquipReply(uint16_t agent, uint32_t instance, size_t slot, Buffer& out) {
    return out.U16(273) && out.VarU32(agent) && out.VarU32(instance) && out.U8(static_cast<uint8_t>(slot));
}
bool SkillBegin(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot, unsigned duration,
                uint32_t target, Buffer& out) {
    // Original unanimated skills use order 122. Its 11-byte payload carries
    // target, skill and slot; channel 1 leaves a timed channel-0 order intact.
    // AgBase requires a positive command lifetime, even without an animation.
    if (!duration)
        return out.U16(34) && out.U16(agent) && out.VarU32(sequence) && out.U8(0x30) && out.VarU32(identity) &&
               out.U8(11) && out.U8(122) && out.U8(0) && out.U32(target) && out.U32(skill) && out.U8(slot) &&
               out.U16(1);
    // Original S34 starts a timed stationary command. Its 13-byte behavior
    // creates ChCliOrder's native skill action (120), animation and slot state.
    return duration && duration <= 65535 && out.U16(34) && out.U16(agent) && out.VarU32(sequence) && out.U8(0x10) &&
           out.VarU32(identity) && out.U8(13) && out.U8(120) && out.U8(0) && out.U32(skill) && out.U8(slot) &&
           out.U8(0) && out.U32(target) && out.U8(0) && out.U16(static_cast<uint16_t>(duration));
}
bool SkillRecharge(uint32_t skill, unsigned remaining, unsigned total, Buffer& out) {
    // Original S609's type 0 is the skill recharge queried by ChCliSkillbar.
    // Both durations are relative milliseconds; charges and slot timers differ.
    if (remaining > total)
        return false;
    Buffer bytes;
    const auto append = [&](uint32_t id) {
        return bytes.U16(609) && bytes.VarU32(57344) && bytes.VarU32(id) && bytes.U8(0) && bytes.VarU32(remaining) &&
               bytes.VarU32(total) && bytes.U16(0);
    };
    if (!append(skill))
        return false;
    // Removing the facet marker restores this slot's base. Project the same
    // display deadline there; only the alternate remains in the saved ledger.
    const auto base = modern::FacetData<167409>::BaseFor(skill);
    if (base && base != skill && !append(base))
        return false;
    return out.Append(bytes.View());
}
bool Decimal64(const std::string& text, uint64_t& out) {
    if (text.empty() || (text.size() > 1 && text[0] == '0'))
        return false;
    uint64_t value = 0;
    for (char c : text) {
        if (c < '0' || c > '9' || value > (UINT64_MAX - static_cast<unsigned>(c - '0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(c - '0');
    }
    out = value;
    return true;
}
bool LoadSkillRecharges(const Character& saved, const std::vector<InitialWeaponSkill>& skills,
                        std::vector<uint64_t>& out, std::string& error) {
    out.assign(skills.size(), 0);
    const auto at = saved.profileFields.find("skill_recharges");
    if (at == saved.profileFields.end() || at->second.empty())
        return true;
    error = "Invalid saved China skill recharges";
    const auto& text = at->second;
    if (text.size() > 1024)
        return false;
    size_t begin = 0;
    while (begin < text.size()) {
        const size_t end = text.find(',', begin), colon = text.find(':', begin);
        const size_t stop = end == std::string::npos ? text.size() : end;
        if (colon == std::string::npos || colon >= stop)
            return false;
        uint64_t id = 0, deadline = 0;
        if (!Decimal64(text.substr(begin, colon - begin), id) || !id || id > UINT32_MAX ||
            !Decimal64(text.substr(colon + 1, stop - colon - 1), deadline) || !deadline)
            return false;
        size_t index = 0;
        for (; index < skills.size(); ++index)
            if (skills[index].skill == id)
                break;
        if (index == skills.size() || !skills[index].rechargeMs || out[index])
            return false;
        out[index] = deadline;
        if (end == std::string::npos)
            break;
        begin = end + 1;
        if (begin == text.size())
            return false;
    }
    error.clear();
    return true;
}
class InventoryActions final : public WorldActions {
  public:
    WorldActionResult PrepareStun(const WorldActionContext& c, std::unique_ptr<PreparedWorldControl>& out) override {
        if (c.peerAgent != peer_.character || !c.sequence)
            return WorldActionResult::Failed;
        const auto impact = c.controlImpactUtc ? c.controlImpactUtc : c.wallNow;
        if (!impact || impact > c.wallNow)
            return WorldActionResult::Failed;
        auto result = std::unique_ptr<PreparedWorldControl>(new PreparedWorldControl);
        result->output.sequences = 0;
        if ((castEndsAt_ || phase_.Active()) &&
            (!modern::SkillEnd(57344, c.sequence, lastSkillIdentity_, result->output.owned) ||
             !modern::SkillEnd(peer_.character, c.sequence, lastSkillIdentity_, result->output.peers) ||
             (pendingRecharge_ && !PendingRecharge(c, result->output.owned))))
            return WorldActionResult::Failed;
        result->commit = [this, impact] {
            inspiring_.CancelIfPending(impact);
            dragonRelease_.CancelIfPending(impact);
            phase_ = {};
            melee_ = {};
            basicLast_ = 0;
            basicUntil_ = 0;
            castEndsAt_ = 0;
            pendingRecharge_ = false;
            maintainedCast_ = 0;
            pendingHeals_.clear();
            greatDwarfAt_ = greatDwarfUtc_ = approachAt_ = approachUtc_ = backstepAt_ = backstepUtc_ = absorptionAt_ =
                absorptionUtc_ = 0;
        };
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult PrepareDemonLeapImpact(const WorldActionContext& c, uint64_t nominal,
                                             std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::DemonLeapImpact::Conditions<167409>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t nominal,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ElementalBlast::Conditions<167409>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t nominal,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::BurstStrengthImpact::Conditions<167409>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t nominal,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::GazeDarkness::Conditions<167409>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 2, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t nominal,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ForcedEngagement::Conditions<167409>(c, nominal, received.conditionState))
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

        if (c.peerAgent != peer_.character || !c.wallNow || skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        struct Prepared final : PreparedHostileEffects {
            bool changed = false;
            InventoryActions& owner;
            SelfEffect effects;
            modern::Hammers<167409> hammers;
            modern::ConditionActions<167409> conditions;
            modern::Stealth<167409> stealth;
            Prepared(InventoryActions& value)
                : owner(value), effects(value.effect_), hammers(value.hammers_), conditions(value.conditions_),
                  stealth(value.stealth_) {}
            void Commit() noexcept override {
                owner.effect_ = std::move(effects);
                owner.effectsNeedSave_ = false;
                owner.hammers_ = std::move(hammers);
                owner.conditions_ = std::move(conditions);
                owner.stealth_ = std::move(stealth);
                if (changed)
                    ++owner.skillVersion_;
            }
        };
        auto next = std::unique_ptr<Prepared>(new Prepared(*this));
        std::vector<combat::LiveBoon> live, removed;
        next->effects.Boons(live);
        next->hammers.Boons(c, live);
        combat::BoonRemovals history;
        if (!history.Load(c.removedBoons) ||
            (operation < 2 && !(stability ? history.SelectContribution(live, 1122, c.wallNow, removed, nominal)
                                          : history.Select(live, combat::BanishEnchantment::RemovalLimit(167409),
                                                           c.wallNow, removed, nominal))))
            return WorldActionResult::Failed;
        const auto ids = combat::BoonRemovals::Instances(removed);
        next->effects.ForgetBoons(ids);
        const auto removedText = history.Text();
        if (!next->hammers.SetRemovedBoons(removedText))
            return WorldActionResult::Failed;
        next->controlBlocked = stability && !removed.empty();
        std::string conditions = c.conditionState;
        if (operation == 0 && !combat::BanishEnchantment::Conditions<167409>(c, nominal, conditions))
            return WorldActionResult::Failed;
        if (operation) {
            modern::ConditionState<167409> ordinary;
            if (!ordinary.Load(conditions))
                return WorldActionResult::Failed;
            ordinary.Expire(c.wallNow);
            conditions = ordinary.Text();
        }
        auto changed = c;
        changed.conditionState = conditions;
        modern::ConditionActions<167409>::Prepared prepared;
        WorldActionOutput output;
        output.sequences = 0;
        if (operation == 2 && nominal <= UINT64_MAX - combat::GazeDarkness::Duration &&
            nominal + combat::GazeDarkness::Duration > c.wallNow && !next->stealth.Suppress(c, output))
            return WorldActionResult::Failed;
        if (!next->conditions.Prepare(changed, 0, level_, prepared, output) ||
            !combat::RemovedBoonMessages<167409>(57344, c.peerAgent, removed, output.owned, output.peers))
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
        const auto effects = prefix + (next->effects.Text());
        if (effects.size() > 8192)
            return WorldActionResult::Failed;
        next->beforeEffects = c.skillEffects;
        next->afterEffects = effects;
        next->beforeConditions = c.conditionState;
        next->afterConditions = conditions;
        next->beforeRemoved = c.removedBoons;
        next->afterRemoved = removedText;
        next->changed = output.owned.Size() || output.peers.Size();
        output.peerEffectVersion = skillVersion_ + (next->changed ? 1 : 0);
        next->output = std::move(output);
        out = std::move(next);
        return WorldActionResult::Handled;
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& context, const ReceivedWorldEffect& request,
                                            std::unique_ptr<PreparedWorldEffect>& out) override {
        return PrepareReceivedEffects(context, {request}, out);
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                             const std::vector<ReceivedWorldEffect>& requests,
                                             std::unique_ptr<PreparedWorldEffect>& out) override {
        if (requests.empty() || requests.size() > 5 || absorptionRecoveryRequired_ ||
            context.peerAgent != peer_.character || skillVersion_ == UINT64_MAX || !context.wallNow ||
            context.skillEffects.size() > 8192)
            return WorldActionResult::Failed;
        auto candidate = effect_;
        if (!rechargesRestored_ && !candidate.Restore(context.now, context.wallNow))
            return WorldActionResult::Failed;
        auto remote = candidate;
        WorldActionOutput output;
        output.sequences = 0;
        std::vector<unsigned> instances;
        std::set<unsigned> refreshed;
        for (const auto& request : requests) {
            if (request.refreshInstance && !refreshed.insert(request.refreshInstance).second)
                return WorldActionResult::Failed;
            unsigned instance = 0, peerInstance = 0;
            if (!candidate.ReceiveProtection(57344, request, context, level_, instance, output.owned) ||
                !remote.ReceiveProtection(peer_.character, request, context, level_, peerInstance, output.peers) ||
                instance != peerInstance)
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
        } else if (!context.skillEffects.empty() && context.skillEffects.compare(0, 2, "1:") &&
                   context.skillEffects.compare(0, 2, "2:"))
            return WorldActionResult::Failed;
        struct Prepared final : PreparedWorldEffect {
            SelfEffect& destination;
            uint64_t& version;
            bool& dirty;
            SelfEffect candidate;
            Prepared(SelfEffect& value, uint64_t& serial, bool& pending, SelfEffect&& next)
                : destination(value), version(serial), dirty(pending), candidate(std::move(next)) {}
            void Commit() noexcept override {
                destination = std::move(candidate);
                dirty = false;
                ++version;
            }
        };
        std::unique_ptr<Prepared> prepared(
            new Prepared(effect_, skillVersion_, effectsNeedSave_, std::move(candidate)));
        prepared->before = context.skillEffects;
        prepared->after = prefix + prepared->candidate.Text();
        if (prepared->after.size() > 8192)
            return WorldActionResult::Failed;
        prepared->instance = instances.front();
        prepared->instances = std::move(instances);
        output.peerEffectVersion = skillVersion_ + 1;
        prepared->output = std::move(output);
        out = std::move(prepared);
        return WorldActionResult::Handled;
    }
    bool PeerCleanup(uint32_t, Buffer& out) const override {
        return missiles_.Cleanup(167409, out, peer_.character) && brand_.Cleanup(peer_.character, out) &&
               stealth_.Cleanup(peer_.character, out) && hammers_.Cleanup(peer_.character, out);
    }
    bool EvadesIncoming(uint64_t now) const override {
        return evasion_.Active(now);
    }
    uint64_t PeerEffectVersion() const override {
        return skillVersion_;
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        if (context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        if (seen)
            return WorldActionResult::Ignored;
        Buffer result;
        if (mediumReported_ && !modern::WeaponSelection(284, peer_.character, medium_, result))
            return WorldActionResult::Failed;
        if (attunementReported_ && !AttunementSelection(peer_.character, attunement_, result))
            return WorldActionResult::Failed;
        if (!legends_.Peer(peer_.character, result))
            return WorldActionResult::Failed;
        if (!effect_.Snapshot(peer_.character, context.now, level_, result))
            return WorldActionResult::Failed;
        if (!hammers_.Snapshot(peer_.character, context.now, level_, energy_, result))
            return WorldActionResult::Failed;
        if (!conditions_.Snapshot(peer_.character, context.wallNow, level_, result))
            return WorldActionResult::Failed;
        if (!(phase_.skill == DemonLeap::Skill && context.now >= phase_.endsAt) &&
            !phase_.Snapshot(phase_.skill == Approach::Skill ? ApproachPhaseContext(context) : context, result))
            return WorldActionResult::Failed;
        if (!missiles_.Snapshot(167409, peer_.character, context.now, result))
            return WorldActionResult::Failed;
        if (!brand_.Snapshot(peer_.character, context.now, result) ||
            !stealth_.Snapshot(peer_.character, context.now, result))
            return WorldActionResult::Failed;
        if (!result.Size())
            return WorldActionResult::Ignored;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    InventoryActions(InventoryState state, PeerIdentity peer, InventoryReports reports, InventoryReports peerReports,
                     std::vector<InitialWeaponSkill> skills, std::array<uint32_t, 68> definitions,
                     std::array<bool, 68> bindings, std::vector<uint64_t> savedRecharges, uint8_t attunement,
                     unsigned level, SelfEffect effect, modern::LegendActions<167409> legends,
                     modern::Hammers<167409> hammers)
        : state_(state), peer_(peer), reports_(std::move(reports)), peerReports_(std::move(peerReports)),
          skills_(std::move(skills)), definitions_(definitions), bindings_(bindings),
          rechargeEndsAt_(skills_.size(), 0), rechargeEndsUtc_(std::move(savedRecharges)),
          resumeReports_(skills_.size(), false), attunement_(attunement), level_(level), effect_(std::move(effect)),
          energy_(std::any_of(skills_.begin(), skills_.end(),
                              [](const InitialWeaponSkill& s) { return s.skill == 26666; })),
          legends_(std::move(legends)), hammers_(std::move(hammers)) {}
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        if (!MeleeDue(now) && !missiles_.CombatDue(now))
            return false;
        Reader r(input);
        uint16_t id = 0;
        return r.U16(id) && id == 23;
    }
    WorldActionResult BasicAttackRequest(const WorldActionContext& c, uint16_t target, uint32_t token,
                                         Buffer& out) const override {
        if (combatRecovery_ || phase_.Active() || (castEndsAt_ && c.now < castEndsAt_) || MeleeDue(c.now))
            return WorldActionResult::Ignored;
        const unsigned desired = c.now < basicUntil_ ? combat::BasicChainNext(167409, basicLast_) : 0;
        for (unsigned pass = 0; pass < 2; ++pass) {
            if (!desired && pass == 0)
                continue;
            for (const auto& s : skills_)
                if (s.requiresWeapon && s.slotType == 3 && s.underwater == (medium_ == 1) && s.minimumLevel <= level_ &&
                    !s.groundTargeted && MissileAvailable(s.skill)) {
                    if (pass == 0 ? s.skill != desired : combat::BasicChainContinuation(s.skill))
                        continue;
                    if (s.skill == modern::StealthRules::Attack && !stealth_.Active(c.now))
                        continue;
                    const auto element = combat::BasicAttunement(s.skill);
                    if (element && element != attunement_)
                        continue;
                    if (s.maximumGroundRange > 0 &&
                        !modern::SkillTargetInRange(c, target, s.minimumGroundRange, s.maximumGroundRange))
                        return WorldActionResult::Ignored;
                    return combat::ForcedEngagement::BasicRequest(167409, peer_.character, target, s.skill, token, out)
                               ? WorldActionResult::Handled
                               : WorldActionResult::Failed;
                }
        }
        return WorldActionResult::Ignored;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        if (combatRecovery_)
            return WorldActionResult::Failed;
        if (absorptionRecoveryRequired_)
            return WorldActionResult::Failed;
        Reader reader(input);
        uint16_t id = 0;
        uint32_t instance = 0, slot = 0;
        uint8_t location = 0;
        if (!input.Valid() || !reader.U16(id))
            return WorldActionResult::Failed;
        if (!RestoreRecharges(context))
            return WorldActionResult::Failed;
        if (id == modern::Legends<167409>::SelectRequest) {
            if (context.peerAgent != peer_.character)
                return WorldActionResult::Failed;
            return legends_.Request(input, context, level_, energy_, skillVersion_, out, LegendPersistence(context));
        }
        uint32_t dodgeToken = 0;
        if (context.persistCombatEndurance && modern::ModernDodgeToken(input, dodgeToken))
            return DodgeRequest(input, dodgeToken, context, out);
        if (!brand_.ObserveMotion(input))
            return WorldActionResult::Failed;
        Facing approachFacing;
        Buffer approachProbe;
        if (China167409Game().DecodeFacing(input, approachFacing) == FacingResult::Present &&
            China167409Game().LocalTeleport(input, {}, 1, approachProbe))
            approachMotion_.assign(input.data, input.data + input.size);
        if (id == 18)
            return MotionMedium(reader, context, out);
        if (id == 23)
            return SkillRequest(reader, context, out);
        const bool isEquip = id == 160 || id == 161;
        if (!isEquip && id != 165 && id != 82)
            return WorldActionResult::Ignored;
        uint8_t quantity = 1;
        if (reader.VarU32(instance) != DecodeStatus::Complete || (id == 82 && !reader.U8(quantity)) ||
            (!isEquip && !reader.U8(location)) || reader.VarU32(slot) != DecodeStatus::Complete || reader.Remaining())
            return WorldActionResult::Failed;
        if (context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        // China retains the native local actor; each peer has its reserved namespace.
        if (instance < 129 || instance >= 129 + state_.locations.size())
            return Notice("That item does not belong to this character.", out);
        const size_t item = static_cast<size_t>(instance - 129);
        if (isEquip && slot != item)
            return Notice("That item cannot be equipped in this slot.", out);
        if (!isEquip && location != 3)
            return Notice("Choose a slot in this character's bag.", out);
        if (id == 82 && quantity != 1)
            return Notice("Choose one item to move.", out);
        if (!isEquip && state_.locations[item] >= 0 && static_cast<unsigned>(state_.locations[item]) == slot)
            return Done(out);
        InventoryState next = state_;
        const auto change = isEquip ? state_.Equip(item, next, bindings_[item])
                                    : (id == 165 ? state_.Unequip(item, slot, next) : state_.Move(item, slot, next));
        switch (change) {
        case InventoryChangeResult::Changed:
            break;
        case InventoryChangeResult::Unchanged:
            return Done(out);
        case InventoryChangeResult::Occupied:
            return Notice("That bag slot is occupied.", out);
        case InventoryChangeResult::InvalidSlot:
            return Notice("That bag slot is unavailable.", out);
        case InventoryChangeResult::Missing:
            return Notice("That item does not belong to this character.", out);
        default:
            return Notice(id == 165 ? "That item is not equipped." : "That item is not in the bag.", out);
        }
        WorldActionOutput result;
        result.sequences = 0;
        const uint32_t remote = static_cast<uint32_t>(peer_.player) * 128 + static_cast<uint32_t>(item) + 1;
        // Reuse April's verified sequence. Native equipment removal updates
        // appearance and releases the client object. Report the same logical
        // instance, definition and dyes before placing it in the bag.
        if (isEquip) {
            // Native S280 clears the bag cell and releases its client object.
            // Restore the same logical object, including its saved binding,
            // before the dedicated equipment attachment updates appearance.
            if (!result.owned.U16(280) || !result.owned.VarU32(57344) || !result.owned.VarU32(instance) ||
                !result.owned.Append(reports_.Get(item, next.bound[item]).View()) ||
                !EquipReply(57344, instance, item, result.owned) ||
                !result.peers.Append(peerReports_.Get(item, next.bound[item]).View()) ||
                !EquipReply(peer_.character, remote, item, result.peers))
                return WorldActionResult::Failed;
        } else {
            if (id == 165 && (!result.owned.U16(295) || !result.owned.VarU32(57344) || !result.owned.VarU32(instance) ||
                              !result.owned.Append(reports_.Get(item, next.bound[item]).View())))
                return WorldActionResult::Failed;
            if (!MoveReply(57344, instance, slot, result.owned))
                return WorldActionResult::Failed;
        }
        // Peers receive the visible equipment removal, without private bag contents.
        if (id == 165 &&
            (!result.peers.U16(295) || !result.peers.VarU32(peer_.character) || !result.peers.VarU32(remote)))
            return WorldActionResult::Failed;
        if (!context.persistInventory || !context.persistInventory(next.Text(), next.BindingText()))
            return Notice("The inventory change could not be saved.", out);
        state_ = next;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (combatRecovery_ || context.peerAgent != peer_.character || !context.sequence)
            return WorldActionResult::Failed;
        if (MeleeDue(context.now))
            return FinishMelee(context, out);
        const int projectile = missiles_.AdvanceCombat(167409, skillVersion_, context, out);
        if (projectile < 0) {
            combatRecovery_ = true;
            return WorldActionResult::Failed;
        }
        if (projectile == 0)
            return WorldActionResult::Handled;
        if (projectile == 2) {
            if (out.peerEffectVersion)
                skillVersion_ = out.peerEffectVersion;
            return WorldActionResult::Handled;
        }
        modern::InitiativeLedger initiative;
        const bool initiativeEnabled = bool(context.persistCombatInitiative);
        if ((!context.initiativeState.empty() && !initiativeEnabled) ||
            (initiativeEnabled && !initiative.Load(context.initiativeState)))
            return WorldActionResult::Failed;
        const auto previousInitiative = initiative.Points();
        if (initiativeEnabled && !initiative.Advance(context.wallNow))
            return WorldActionResult::Failed;
        bool initiativeUpdate = initiativeEnabled && (!initiativeAnnounced_ || context.initiativeState.empty() ||
                                                      initiative.Points() != previousInitiative);
        modern::EnduranceLedger resource;
        const bool enduranceEnabled = (energy_.Enabled() || enduranceAnnounced_ || !context.enduranceState.empty()) &&
                                      bool(context.persistCombatEndurance);
        if ((!context.enduranceState.empty() && !enduranceEnabled) ||
            (enduranceEnabled && (!resource.Load(context.enduranceState) || !resource.Advance(context.wallNow))))
            return WorldActionResult::Failed;
        if (absorptionRecoveryRequired_)
            return WorldActionResult::Failed;
        if (!RestoreRecharges(context))
            return WorldActionResult::Failed;
        auto nextEffect = effect_;
        float regenerationHealth = 0, regenerationDelta = 0;
        bool regenerationChanged = false;
        if (!modern::ReadHealthState(context.healthState, regenerationHealth) ||
            !nextEffect.PrepareRegeneration(context.wallNow, regenerationHealth, regenerationDelta,
                                            regenerationChanged))
            return WorldActionResult::Failed;
        if (greatDwarfAt_ && (regenerationHealth <= 0 || legends_.Active() != modern::GreatDwarfData<167409>::Legend ||
                              greatDwarfWater_ != (medium_ == 1)))
            greatDwarfAt_ = greatDwarfUtc_ = 0;
        const bool phaseReady = phase_.Due(context.now);
        const auto available = [&](uint32_t skill) { return MissileAvailable(skill); };
        const bool missileReady = missiles_.Due(context.now, available);
        const bool brandReady = brand_.Due();
        const bool stealthReady = stealth_.Due();
        const bool energyReady = energy_.Due(context.now);
        const bool castDone = !phase_.Active() && castEndsAt_ && context.now >= castEndsAt_;
        const bool rechargeReady = pendingRecharge_ && context.now >= rechargeStartsAt_;
        const bool healsReady = modern::HealsDue(pendingHeals_, context.now);
        const bool selfSourceHealDue =
            std::any_of(pendingHeals_.begin(), pendingHeals_.end(), [&](const modern::PendingHeal& heal) {
                return (heal.skill == 13027 || heal.skill == 5834) && context.now >= heal.startsAt;
            });
        const bool retryableHealDue =
            std::any_of(pendingHeals_.begin(), pendingHeals_.end(), [&](const modern::PendingHeal& heal) {
                return (heal.skill == 26937 || heal.skill == 13027 || heal.skill == 5834 ||
                        (heal.skill == 5503 || heal.skill == 13050)) &&
                       context.now >= heal.startsAt;
            });
        float absorptionHealth = 0;
        if (absorptionAt_ && !modern::ReadHealthState(context.healthState, absorptionHealth))
            return WorldActionResult::Failed;
        const bool cancelAbsorption =
            absorptionAt_ &&
            (absorptionHealth == 0 || legends_.Active() != Absorption::Legend() || absorptionWater_ != (medium_ == 1));
        const bool absorptionDue = absorptionAt_ && !cancelAbsorption && context.now >= absorptionAt_;
        modern::PainAbsorptionPlan absorptionPlan;
        auto conditionContext = context;
        if (absorptionDue) {
            std::vector<WorldConditionActor> allies;
            std::string error;
            if (!context.conditionAllies || !context.conditionAllies(allies) ||
                !Absorption::Prepare(peer_.character, context.position, context.conditionState, allies, absorptionUtc_,
                                     context.wallNow, absorptionPlan, error))
                return WorldActionResult::Failed;
            conditionContext.conditionState = absorptionPlan.ownConditions;
        }
        float backstepHealth = 0;
        if (backstepAt_ && !modern::ReadHealthState(context.healthState, backstepHealth))
            return WorldActionResult::Failed;
        const bool cancelBackstep =
            backstepAt_ && (backstepHealth == 0 || legends_.Active() != 15 || backstepWater_ != (medium_ == 1));
        float leapHealth = 0;
        const bool leapActive = phase_.Active() && phase_.skill == DemonLeap::Skill;
        if (leapActive && !modern::ReadHealthState(context.healthState, leapHealth))
            return WorldActionResult::Failed;
        const bool cancelLeap =
            leapActive && (leapHealth == 0 || legends_.Active() != 17 || leapWater_ != (medium_ == 1) ||
                           (phase_.index == 0 && !DemonLeap::ValidPoint(context, leapPoint_)));
        const bool backstepDue = backstepAt_ && !cancelBackstep && context.now >= backstepAt_;
        if (backstepDue) {
            if (enduranceEnabled)
                resource.Gain(Backstep::EnduranceGain());
            modern::ConditionState<167409> candidate;
            if (!candidate.Load(conditionContext.conditionState))
                return WorldActionResult::Failed;
            // Explicit local policy: only instances active at the accepted
            // trigger are eligible; retries do not consume later contributions.
            candidate.RemoveDefinitions(Backstep::Definitions(), backstepUtc_);
            conditionContext.conditionState = candidate.Text();
        }
        bool selfSourceConditions = false;
        if (healsReady && regenerationHealth > 0)
            for (const auto& heal : pendingHeals_) {
                if (heal.skill != 13027 || context.now < heal.startsAt)
                    continue;
                const uint64_t elapsed = context.now - heal.startsAt;
                if (elapsed >= context.wallNow)
                    return WorldActionResult::Failed;
                modern::ConditionState<167409> candidate;
                if (!candidate.Load(conditionContext.conditionState))
                    return WorldActionResult::Failed;
                // Compatible local removal policy, not a recovered original operation:
                // own datatype4 primary refs737/723/736/861; preserve all other types.
                candidate.RemoveDefinitions({737, 723, 736, 861}, context.wallNow - elapsed);
                conditionContext.conditionState = candidate.Text();
                selfSourceConditions = true;
            }
        modern::ConditionActions<167409>::Prepared nextConditions;
        WorldActionOutput conditionChanges;
        if (!conditions_.Prepare(conditionContext, modern::DwarfConditionRemovalDue(pendingHeals_, context), level_,
                                 nextConditions, conditionChanges) ||
            ((nextConditions.changed || absorptionDue) && skillVersion_ == UINT64_MAX))
            return WorldActionResult::Failed;
        // Health/condition transactions take priority; the field retains its
        // nominal pulse deadline and retries on the next Tick, never two writes.
        nextConditions.save = nextConditions.save || backstepDue || selfSourceConditions;
        if (!regenerationChanged && !cancelBackstep && !cancelLeap && !healsReady && !absorptionDue &&
            !nextConditions.changed && !nextConditions.save) {
            bool approachHandled = false;
            const auto approach = AdvanceApproach(context, out, approachHandled);
            if (approachHandled)
                return approach;
            bool riteHandled = false;
            const auto rite = AdvanceGreatDwarfAllies(context, out, riteHandled);
            if (riteHandled)
                return rite;
            bool releaseHandled = false;
            const auto release = AdvanceDragonRelease(context, out, releaseHandled);
            if (releaseHandled)
                return release;
            bool handled = false;
            const auto field = AdvanceInspiring(context, out, handled);
            if (handled)
                return field;
            bool facetHandled = false;
            const auto facetPulse = AdvanceFacetPulse(context, out, facetHandled);
            if (facetHandled)
                return facetPulse;
        }
        bool resumeReady = false;
        for (bool pending : resumeReports_)
            if (pending)
                resumeReady = true;
        if (context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        auto peerEffect = effect_;
        bool effectChanged = false, peerChanged = false;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        bool hammersChanged = false;
        if (!nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, result, hammersChanged))
            return WorldActionResult::Failed;
        if (effect_.ActiveStance(context.now) && ((mediumReported_ && medium_ != 1) || state_.locations[24] != -1)) {
            if (!nextEffect.Consume(57344, result.owned) || !peerEffect.Consume(peer_.character, result.peers))
                return WorldActionResult::Failed;
            effectChanged = true;
        } else if (!nextEffect.Advance(57344, context.now, level_, result.owned, effectChanged,
                                       greatDwarfAt_ ? greatDwarfUtc_ + RiteAllies::Duration : 0) ||
                   !peerEffect.Advance(peer_.character, context.now, level_, result.peers, peerChanged,
                                       greatDwarfAt_ ? greatDwarfUtc_ + RiteAllies::Duration : 0))
            return WorldActionResult::Failed;
        bool saveEffect = effectsNeedSave_ || nextEffect.Text() != effect_.Text() || nextHammers.NeedsSave() ||
                          (nextHammers.Tracked() && (energyReady || hammersChanged));
        const bool enduranceUpdate = enduranceEnabled && !absorptionDue &&
                                     (!enduranceAnnounced_ || backstepDue || resource.Points() != endurancePoints_);
        if (enduranceEnabled && backstepDue && absorptionDue)
            return WorldActionResult::Failed;
        if (initiativeUpdate && absorptionDue)
            initiativeUpdate = false;
        if (!regenerationChanged && !castDone && !phaseReady && !rechargeReady && !resumeReady && !effectChanged &&
            !saveEffect && !missileReady && !brandReady && !stealthReady && !energyReady && !healsReady &&
            !legends_.Due() && !nextConditions.changed && !nextConditions.save && !absorptionDue && !cancelAbsorption &&
            !backstepDue && !cancelBackstep && !cancelLeap && !enduranceUpdate && !initiativeUpdate)
            return WorldActionResult::Ignored;
        auto nextLegends = legends_;
        if (!nextLegends.Initial(context, result))
            return WorldActionResult::Failed;
        if (effectChanged || missileReady || brandReady || stealthReady || hammersChanged) {
            if (skillVersion_ == UINT64_MAX)
                return WorldActionResult::Failed;
        }
        auto nextPhase = phase_;
        if (cancelBackstep || cancelLeap) {
            if (!context.sequence || context.sequence == UINT32_MAX ||
                !modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
            nextPhase = modern::PhasedSkill{};
        } else if (phaseReady) {
            if (phase_.skill == DemonLeap::Skill) {
                if (!AdvanceDemonLeapPhases(context, nextPhase, result))
                    return WorldActionResult::Failed;
            } else if (phase_.skill == Backstep::Skill) {
                if (!AdvanceBackstepPhases(context, nextPhase, result))
                    return WorldActionResult::Failed;
            } else if (!nextPhase.Advance(phase_.skill == Approach::Skill ? ApproachPhaseContext(context) : context,
                                          result))
                return WorldActionResult::Failed;
        }
        if (castDone && healCast_) {
            if (!context.sequence || context.sequence == UINT32_MAX ||
                !modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
        }
        auto nextMissiles = missiles_;
        auto nextBrand = brand_;
        auto nextStealth = stealth_;
        if (energyReady && !nextEnergy.Advance(context.now, result))
            return WorldActionResult::Failed;
        if (missileReady && !nextMissiles.Advance(167409, context, available, result))
            return WorldActionResult::Failed;
        const auto beforeBrand = result.peers.Size();
        if (brandReady && !nextBrand.Advance(context, MissileAvailable(modern::RevenantBrandRules::Base), result))
            return WorldActionResult::Failed;
        const bool brandChanged = result.peers.Size() != beforeBrand;
        if (regenerationHealth <= 0)
            nextStealth.CancelPending();
        const auto beforeStealth = result.peers.Size();
        if (stealthReady && !nextStealth.Advance(context, available, result))
            return WorldActionResult::Failed;
        const bool stealthChanged = result.peers.Size() != beforeStealth;
        for (size_t i = 0; i < skills_.size(); ++i)
            if (resumeReports_[i]) {
                const uint64_t remaining = rechargeEndsAt_[i] > context.now ? rechargeEndsAt_[i] - context.now : 0;
                const unsigned total = skills_[i].rechargeMs;
                if (!SkillRecharge(skills_[i].skill, remaining > total ? total : static_cast<unsigned>(remaining),
                                   total, result.owned))
                    return WorldActionResult::Failed;
            }
        if (rechargeReady && !PendingRecharge(context, result.owned))
            return WorldActionResult::Failed;
        std::vector<modern::PendingHeal> nextHeals;
        float previousHealth = 0, currentHealth = 0;
        if (healsReady && !modern::PrepareSelfHeals<167409>(pendingHeals_, context, peer_.character, nextHeals,
                                                            previousHealth, currentHealth, result))
            return WorldActionResult::Failed;
        uint64_t assassinActivation = 0;
        if (healsReady && previousHealth > 0 && context.persistAssassinActivation)
            for (const auto& heal : pendingHeals_) {
                if (heal.skill != 26937 || context.now < heal.startsAt)
                    continue;
                const auto elapsed = context.now - heal.startsAt;
                if (assassinActivation || elapsed >= context.wallNow)
                    return WorldActionResult::Failed;
                assassinActivation = context.wallNow - elapsed;
            }
        if (healsReady && previousHealth > 0)
            for (const auto& heal : pendingHeals_) {
                if (heal.skill != modern::DwarfEffectRules::Skill || context.now < heal.startsAt)
                    continue;
                const uint64_t elapsed = context.now - heal.startsAt;
                if (elapsed >= modern::DwarfEffectRules::DurationMs)
                    continue;
                if (skillVersion_ == UINT64_MAX ||
                    !nextEffect.Start(57344, heal.skill, context.now, context.wallNow, level_, result.owned, elapsed) ||
                    !peerEffect.Start(peer_.character, heal.skill, context.now, context.wallNow, level_, result.peers,
                                      elapsed))
                    return WorldActionResult::Failed;
                effectChanged = true;
                saveEffect = true;
            }
        if (healsReady && previousHealth > 0)
            for (const auto& heal : pendingHeals_) {
                if ((heal.skill != 13027 && heal.skill != 5834) || context.now < heal.startsAt)
                    continue;
                const uint64_t elapsed = context.now - heal.startsAt;
                for (unsigned buff : {717u, 718u, 719u}) {
                    if (heal.skill == 13027 && buff != 718)
                        continue;
                    const unsigned duration = heal.skill == 13027 ? 6000 : buff == 717 ? 2000 : 4000;
                    if (elapsed >= duration)
                        continue;
                    if (skillVersion_ == UINT64_MAX ||
                        !nextEffect.StartPersonalBenefit(57344, heal.skill, buff, context.now, context.wallNow, level_,
                                                         result.owned, elapsed) ||
                        !peerEffect.StartPersonalBenefit(peer_.character, heal.skill, buff, context.now,
                                                         context.wallNow, level_, result.peers, elapsed))
                        return WorldActionResult::Failed;
                    effectChanged = true;
                    saveEffect = true;
                }
            }
        if (backstepDue) {
            if (context.wallNow < backstepUtc_ || backstepUtc_ > UINT64_MAX - Backstep::BoonDuration())
                return WorldActionResult::Failed;
            const auto elapsed = context.wallNow - backstepUtc_;
            if (elapsed < Backstep::BoonDuration()) {
                if (skillVersion_ == UINT64_MAX ||
                    !nextEffect.StartPersonalBenefit(57344, Backstep::Skill, Backstep::Boon(), context.now,
                                                     context.wallNow, level_, result.owned, elapsed) ||
                    !peerEffect.StartPersonalBenefit(peer_.character, Backstep::Skill, Backstep::Boon(), context.now,
                                                     context.wallNow, level_, result.peers, elapsed))
                    return WorldActionResult::Failed;
                effectChanged = true;
                saveEffect = true;
            }
        }
        if (absorptionDue)
            for (const auto& grant : absorptionPlan.grants) {
                if (!nextEffect.GrantAbsorption(57344, grant, context, level_, result.owned) ||
                    !peerEffect.GrantAbsorption(peer_.character, grant, context, level_, result.peers))
                    return WorldActionResult::Failed;
                effectChanged = true;
                saveEffect = true;
            }
        if (regenerationChanged) {
            if (!healsReady) {
                if (!modern::ReadHealthState(context.healthState, currentHealth))
                    return WorldActionResult::Failed;
            }
            const float after = currentHealth > 0
                                    ? (std::min)(modern::kPublishedMaximumHealth, currentHealth + regenerationDelta)
                                    : currentHealth;
            const float delta = after - currentHealth;
            currentHealth = after;
            if (delta > 0 && (!modern::HealthChange<167409>(57344, 718, delta, result.owned) ||
                              !modern::HealthChange<167409>(peer_.character, 718, delta, result.peers)))
                return WorldActionResult::Failed;
        }
        const bool saveHealth =
            regenerationChanged || selfSourceHealDue || (healsReady && currentHealth != previousHealth);
        if (!result.owned.Append(conditionChanges.owned.View()) ||
            !result.peers.Append(conditionChanges.peers.View()) ||
            (result.targetPeerAgent && !result.targetView.Append(conditionChanges.peers.View())))
            return WorldActionResult::Failed;
        if (initiativeUpdate && !modern::InitiativeCurrent(167409, 57344, initiative, result.owned))
            return WorldActionResult::Failed;
        if (enduranceUpdate && !modern::EnduranceCurrent(167409, 57344, resource.Points(), result.owned))
            return WorldActionResult::Failed;
        if (absorptionDue) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health) ||
                !context.persistConditionTransfer)
                return WorldActionResult::Failed;
            const auto committed = context.persistConditionTransfer(
                RechargeText(rechargeEndsUtc_), nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                nextConditions.state.Text(), absorptionPlan.donors);
            if (committed == WorldConditionCommit::RecoveryRequired) {
                absorptionRecoveryRequired_ = true;
                return WorldActionResult::Failed;
            }
            if (committed == WorldConditionCommit::NotApplied)
                return Done(out);
        } else if (assassinActivation) {
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            if (!context.persistAssassinActivation(
                    RechargeText(rechargeEndsUtc_), nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                    nextConditions.state.Text(), enduranceUpdate ? resource.Text() : context.enduranceState,
                    assassinActivation))
                return Done(out);
        } else if (initiativeUpdate) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            const bool stored =
                enduranceUpdate
                    ? (context.persistCombatResources &&
                       context.persistCombatResources(RechargeText(rechargeEndsUtc_),
                                                      nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                                                      nextConditions.state.Text(), resource.Text(), initiative.Text()))
                    : context.persistCombatInitiative(RechargeText(rechargeEndsUtc_),
                                                      nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                                                      nextConditions.state.Text(), initiative.Text());
            if (!stored)
                return Done(out);
        } else if (enduranceUpdate) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            if (!context.persistCombatEndurance(RechargeText(rechargeEndsUtc_),
                                                nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                                                nextConditions.state.Text(), resource.Text()))
                return Done(out);
        } else if (nextConditions.save) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health) ||
                !context.persistCombatConditions)
                return WorldActionResult::Failed;
            if (!context.persistCombatConditions(RechargeText(rechargeEndsUtc_),
                                                 nextHammers.Text(nextEffect.Text(), nextEnergy, context), health,
                                                 nextConditions.state.Text()))
                return (retryableHealDue || regenerationChanged) ? Done(out) : WorldActionResult::Failed;
        } else if (saveHealth) {
            std::string health;
            if (!context.ReserveSequences(result.sequences) || !modern::WriteHealthState(currentHealth, health) ||
                !context.persistCombatState)
                return WorldActionResult::Failed;
            if (!context.persistCombatState(RechargeText(rechargeEndsUtc_),
                                            nextHammers.Text(nextEffect.Text(), nextEnergy, context), health))
                return (retryableHealDue || regenerationChanged) ? Done(out) : WorldActionResult::Failed;
        } else if (saveEffect && (!context.ReserveSequences(result.sequences) || !context.persistSkillState ||
                                  !context.persistSkillState(RechargeText(rechargeEndsUtc_),
                                                             nextHammers.Text(nextEffect.Text(), nextEnergy, context))))
            return WorldActionResult::Failed;
        if (healsReady)
            pendingHeals_ = std::move(nextHeals);
        effect_ = std::move(nextEffect);
        effectsNeedSave_ = false;
        if (effectChanged || missileReady || brandChanged || stealthChanged || nextConditions.changed || hammersChanged)
            ++skillVersion_;
        if ((effectChanged || missileReady || brandChanged || stealthChanged || nextConditions.changed ||
             hammersChanged) &&
            result.peers.Size())
            result.peerEffectVersion = skillVersion_;
        if (absorptionDue || cancelAbsorption)
            absorptionAt_ = absorptionUtc_ = 0;
        if (initiativeUpdate)
            initiativeAnnounced_ = true;
        if (enduranceUpdate) {
            enduranceAnnounced_ = true;
            endurancePoints_ = resource.Points();
        }
        if (backstepDue || cancelBackstep)
            backstepAt_ = backstepUtc_ = 0;
        if (cancelBackstep || cancelLeap)
            castEndsAt_ = 0;
        conditions_.Commit(std::move(nextConditions));
        missiles_ = std::move(nextMissiles);
        brand_ = std::move(nextBrand);
        stealth_ = std::move(nextStealth);
        energy_ = std::move(nextEnergy);
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        legends_ = std::move(nextLegends);
        if (rechargeReady)
            pendingRecharge_ = false;
        if (castDone || (phaseReady && !nextPhase.Active())) {
            castEndsAt_ = 0;
            healCast_ = false;
        }
        phase_ = std::move(nextPhase);
        for (size_t i = 0; i < resumeReports_.size(); ++i)
            resumeReports_[i] = false;
        out = std::move(result);
        return WorldActionResult::Handled;
    }

  private:
    bool initiativeAnnounced_ = false;
    bool enduranceAnnounced_ = false;
    unsigned endurancePoints_ = 100;
    uint32_t lastDodgeToken_ = 0;
    combat::DodgeEvasion evasion_;
    WorldActionResult DodgeRequest(ByteView input, uint32_t token, const WorldActionContext& context,
                                   WorldActionOutput& out) {
        const auto& game = China167409Game();
        CatalogPosition position;
        Facing facing;
        float health = 0;
        if (!combat::DodgeEvasion::CanBegin(context.now) || context.peerAgent != peer_.character || !context.sequence ||
            context.sequence > UINT32_MAX - 2 || token <= lastDodgeToken_ || skillVersion_ == UINT64_MAX ||
            !game.NeedsActionTickBeforeMotion(input) || (backstepAt_ && context.now >= backstepAt_))
            return Done(out);
        if (game.DecodePosition(input, position) == PositionResult::Invalid ||
            game.DecodeFacing(input, facing) == FacingResult::Invalid ||
            !modern::ReadHealthState(context.healthState, health) || !RestoreRecharges(context))
            return WorldActionResult::Failed;
        if (!health)
            return Done(out);
        modern::EnduranceLedger resource;
        if (!resource.Load(context.enduranceState) || !resource.Advance(context.wallNow))
            return WorldActionResult::Failed;
        if (!resource.SpendDodge())
            return Done(out);
        WorldActionOutput result;
        result.sequences = 1;
        if (phase_.Active() || castEndsAt_) {
            if (!modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers))
                return WorldActionResult::Failed;
            ++result.sequences;
        }
        if (pendingRecharge_ && !PendingRecharge(context, result.owned))
            return WorldActionResult::Failed;
        Buffer own, peers;
        const auto sequence = context.sequence + result.sequences - 1;
        if (game.OwnMotion(input, sequence, static_cast<uint32_t>(context.now & 0x7fffffffu), own) ==
                RelayResult::Invalid ||
            game.PeerMotion(input, peer_.character, sequence, static_cast<uint32_t>(context.now & 0x7fffffffu),
                            peers) == RelayResult::Invalid ||
            !result.owned.Append(own.View()) || !result.peers.Append(peers.View()) ||
            !modern::EnduranceCurrent(167409, 57344, resource.Points(), result.owned))
            return WorldActionResult::Failed;
        auto nextEffect = effect_;
        nextEffect.CancelPending(context.now);
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!context.persistCombatEndurance(RechargeText(rechargeEndsUtc_),
                                            hammers_.Text(nextEffect.Text(), energy_, context), context.healthState,
                                            context.conditionState, resource.Text()))
            return Done(out);
        effect_ = std::move(nextEffect);
        phase_ = modern::PhasedSkill{};
        castEndsAt_ = 0;
        pendingRecharge_ = false;
        missiles_.CancelPending();
        brand_.CancelWarp();
        stealth_.CancelPending();
        modern::CancelPendingHeals(pendingHeals_, context.now);
        absorptionAt_ = absorptionUtc_ = approachAt_ = approachUtc_ = 0;
        backstepAt_ = backstepUtc_ = greatDwarfAt_ = greatDwarfUtc_ = 0;
        dragonRelease_.Cancel();
        CancelAssassinInitialHeal();
        evasion_.Begin(context.now);
        lastDodgeToken_ = token;
        enduranceAnnounced_ = true;
        endurancePoints_ = resource.Points();
        result.peerEffectVersion = ++skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    // Ground orientation/cap are local policy. Skip elapsed movement instead
    // of replaying it after its nominal end; snapshots retain the emitted path.
    bool AdvanceDemonLeapPhases(const WorldActionContext& c, modern::PhasedSkill& next,
                                WorldActionOutput& result) const {
        if (!next.Active() || castEndsAt_ < DemonLeap::Animation || !c.sequence ||
            result.sequences >= UINT32_MAX - c.sequence)
            return false;
        const auto start = castEndsAt_ - DemonLeap::Animation;
        const auto sequence = c.sequence + result.sequences;
        if (c.now >= castEndsAt_) {
            if (next.index != 2 || !next.emitted) {
                if (!modern::SkillEnd(57344, sequence, next.identity, result.owned) ||
                    !modern::SkillEnd(peer_.character, sequence, next.identity, result.peers))
                    return false;
                ++result.sequences;
            }
            next = modern::PhasedSkill{};
            return true;
        }
        const bool movement = c.now < start + DemonLeap::MoveEnd;
        next.index = movement ? 1 : 2;
        const auto end = movement ? start + DemonLeap::MoveEnd : castEndsAt_;
        auto& stage = next.phases[next.index];
        stage.durationMs = static_cast<uint16_t>(end - c.now);
        auto aimed = c;
        if (movement) {
            std::array<float, 3> direction;
            float speed = 0;
            bool moving = false;
            if (!DemonLeap::Aim(c, leapPoint_, leapWater_, stage.durationMs, direction, speed, moving))
                return false;
            aimed.hasFacing = true;
            aimed.facing = {direction[0], direction[1], direction[2]};
            stage.type = moving ? 1 : 0;
            stage.movement[8] = stage.movement[9] = speed;
        }
        if (!next.Emit(aimed, sequence, result, false))
            return false;
        ++result.sequences;
        return true;
    }
    bool AdvanceBackstepPhases(const WorldActionContext& c, modern::PhasedSkill& next,
                               WorldActionOutput& result) const {
        if (!next.Active() || castEndsAt_ < Backstep::Animation)
            return false;
        const auto start = castEndsAt_ - Backstep::Animation;
        next.endsAt = start + (backstepWater_ ? (next.index == 0 ? 800 : 1000)
                                              : (next.index == 0   ? 500
                                                 : next.index == 1 ? 800
                                                                   : 1000));
        while (next.Due(c.now)) {
            if (!c.sequence || result.sequences >= UINT32_MAX - c.sequence)
                return false;
            auto nominal = c;
            nominal.sequence = c.sequence + result.sequences;
            nominal.now = next.endsAt;
            if (next.index + 1 < next.phases.size()) {
                auto& stage = next.phases[next.index + 1];
                const auto end = next.endsAt + stage.durationMs;
                if (end > c.now) {
                    stage.durationMs = static_cast<uint16_t>(end - c.now);
                    nominal.now = c.now;
                }
            }
            WorldActionOutput emitted;
            emitted.sequences = 0;
            if (!next.Advance(nominal, emitted) || !result.owned.Append(emitted.owned.View()) ||
                !result.peers.Append(emitted.peers.View()))
                return false;
            result.sequences += emitted.sequences;
        }
        return true;
    }
    WorldActionContext ApproachPhaseContext(const WorldActionContext& c) const {
        auto next = c;
        const bool water = medium_ == 1;
        // Apply the same local64-unit endpoint to authored movement and final relocation.
        next.characterTargetPosition = [c, water](uint32_t target, CatalogPosition& point) {
            bool relocate = false;
            return Approach::Destination(c, target, water, point, relocate);
        };
        return next;
    }
    WorldActionResult AdvanceApproach(const WorldActionContext& c, WorldActionOutput& out, bool& handled) {
        handled = false;
        if (!approachAt_)
            return WorldActionResult::Ignored;
        if (c.peerAgent != peer_.character || skillVersion_ == UINT64_MAX) {
            handled = true;
            return WorldActionResult::Failed;
        }
        float health = 0;
        CatalogPosition destination;
        bool relocate = false;
        if (!modern::ReadHealthState(c.healthState, health)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        if (health <= 0 || legends_.Active() != 15 || approachWater_ != (medium_ == 1) ||
            (c.now >= approachAt_ && c.now - approachAt_ >= Approach::Duration) ||
            !Approach::Destination(c, approachTarget_, approachWater_, destination, relocate)) {
            handled = true;
            WorldActionOutput cancelled;
            cancelled.sequences = 1;
            if (!c.sequence || c.sequence == UINT32_MAX ||
                !modern::SkillEnd(57344, c.sequence, lastSkillIdentity_, cancelled.owned) ||
                !modern::SkillEnd(peer_.character, c.sequence, lastSkillIdentity_, cancelled.peers) ||
                (pendingRecharge_ && !PendingRecharge(c, cancelled.owned)) || !c.ReserveSequences(1))
                return WorldActionResult::Failed;
            approachAt_ = approachUtc_ = 0;
            phase_ = modern::PhasedSkill{};
            castEndsAt_ = 0;
            pendingRecharge_ = false;
            cancelled.peerEffectVersion = ++skillVersion_;
            out = std::move(cancelled);
            return WorldActionResult::Handled;
        }
        if (c.now < approachAt_)
            return WorldActionResult::Ignored;
        handled = true;
        if (c.peerAgent != peer_.character || !c.wallNow || skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        auto nextEnergy = energy_;
        auto nextHammers = hammers_;
        bool hammersChanged = false;
        if (!nextHammers.Advance(c, legends_.Active(), level_, nextEnergy, result, hammersChanged) ||
            (nextEnergy.Due(c.now) && !nextEnergy.Advance(c.now, result)))
            return WorldActionResult::Failed;
        // Catch up the own phase schedule before relocation. Advance normally
        // schedules from its input clock, so use nominal deadlines for elapsed
        // stages and only the remaining duration for the currently active stage.
        const auto beforePhasePeers = result.peers.Size();
        auto nextPhase = phase_;
        if (nextPhase.Active())
            nextPhase.endsAt = approachAt_ - Approach::Activation +
                               (nextPhase.index == 0   ? 367
                                : nextPhase.index == 1 ? 567
                                                       : Approach::Animation);
        while (nextPhase.Due(c.now)) {
            if (!c.sequence || c.sequence == UINT32_MAX || result.sequences >= UINT32_MAX - c.sequence)
                return WorldActionResult::Failed;
            auto phaseContext = ApproachPhaseContext(c);
            phaseContext.sequence = c.sequence + result.sequences;
            phaseContext.now = nextPhase.endsAt;
            if (nextPhase.index + 1 < nextPhase.phases.size()) {
                auto& stage = nextPhase.phases[nextPhase.index + 1];
                if (nextPhase.endsAt > UINT64_MAX - stage.durationMs)
                    return WorldActionResult::Failed;
                const auto end = nextPhase.endsAt + stage.durationMs;
                if (end > c.now) {
                    stage.durationMs = static_cast<uint16_t>(end - c.now);
                    phaseContext.now = c.now;
                }
            }
            WorldActionOutput emitted;
            emitted.sequences = 0;
            if (!nextPhase.Advance(phaseContext, emitted))
                return WorldActionResult::Failed;
            if (emitted.targetPeerAgent && !result.targetPeerAgent && beforePhasePeers &&
                !result.targetView.Append({result.peers.Data(), beforePhasePeers}))
                return WorldActionResult::Failed;
            if (!result.owned.Append(emitted.owned.View()) || !result.peers.Append(emitted.peers.View()) ||
                !result.targetView.Append(emitted.targetView.View()))
                return WorldActionResult::Failed;
            if (emitted.targetPeerAgent)
                result.targetPeerAgent = emitted.targetPeerAgent;
            result.sequences += emitted.sequences;
        }
        const auto phasePeerBytes = result.peers.Size();
        if (relocate) {
            // Game teleport encoders replace their output; preserve the staged batch.
            Buffer ownTeleport, peerTeleport;
            if (approachMotion_.empty() || !c.sequence || c.sequence > UINT32_MAX - 2 ||
                result.sequences > UINT32_MAX - c.sequence - 2 ||
                !China167409Game().LocalTeleport({approachMotion_.data(), approachMotion_.size()}, destination,
                                                 c.sequence + result.sequences, ownTeleport) ||
                !China167409Game().PeerTeleport({approachMotion_.data(), approachMotion_.size()}, destination,
                                                peer_.character, c.sequence + result.sequences, peerTeleport) ||
                !result.owned.Append(ownTeleport.View()) || !result.peers.Append(peerTeleport.View()))
                return WorldActionResult::Failed;
            result.sequences += 2;
            result.relocate = true;
            result.destination = destination;
        }
        // Deadline is the accepted cast's trigger, never a fresh five seconds on retry.
        const uint64_t elapsed = c.now - approachAt_;
        auto own = effect_, remote = effect_;
        bool changed = false, peerChanged = false;
        if (!own.Advance(57344, c.now, level_, result.owned, changed) ||
            !remote.Advance(peer_.character, c.now, level_, result.peers, peerChanged))
            return WorldActionResult::Failed;
        if (elapsed < Approach::Duration) {
            if (!own.Start(57344, Approach::Skill, c.now, approachUtc_ + elapsed, level_, result.owned, elapsed) ||
                !remote.Start(peer_.character, Approach::Skill, c.now, approachUtc_ + elapsed, level_, result.peers,
                              elapsed))
                return WorldActionResult::Failed;
        }
        if (result.targetPeerAgent && result.peers.Size() > phasePeerBytes &&
            !result.targetView.Append({result.peers.Data() + phasePeerBytes, result.peers.Size() - phasePeerBytes}))
            return WorldActionResult::Failed;
        if (!c.ReserveSequences(result.sequences) || !c.persistSkillState)
            return WorldActionResult::Failed;
        if (!c.persistSkillState(RechargeText(rechargeEndsUtc_), nextHammers.Text(own.Text(), nextEnergy, c)))
            return Done(out);
        effect_ = std::move(own);
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        phase_ = std::move(nextPhase);
        if (!phase_.Active())
            castEndsAt_ = 0;
        approachAt_ = approachUtc_ = 0;
        ++skillVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    void CancelAssassinInitialHeal() {
        pendingHeals_.erase(std::remove_if(pendingHeals_.begin(), pendingHeals_.end(),
                                           [](const modern::PendingHeal& heal) { return heal.skill == 26937; }),
                            pendingHeals_.end());
    }
    WorldActionResult AdvanceGreatDwarfAllies(const WorldActionContext& c, WorldActionOutput& out, bool& handled) {
        handled = false;
        if (!greatDwarfAt_)
            return WorldActionResult::Ignored;
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        if (health <= 0 || legends_.Active() != modern::GreatDwarfData<167409>::Legend ||
            greatDwarfWater_ != (medium_ == 1)) {
            greatDwarfAt_ = greatDwarfUtc_ = 0;
            return WorldActionResult::Ignored;
        }
        if (c.now < greatDwarfAt_)
            return WorldActionResult::Ignored;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (!nextHammers.Advance(c, legends_.Active(), level_, nextEnergy, result, changed)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        std::vector<WorldConditionActor> allies;
        if (!c.conditionAllies || !c.conditionAllies(allies)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        modern::GreatDwarfAlliesPlan plan;
        const auto status = RiteAllies::Prepare(c, greatDwarfUtc_, allies, plan);
        if (status == modern::GreatDwarfAlliesStatus::None)
            return WorldActionResult::Ignored;
        if (status == modern::GreatDwarfAlliesStatus::Expired) {
            greatDwarfAt_ = greatDwarfUtc_ = 0;
            return WorldActionResult::Ignored;
        }
        handled = true;
        if (status != modern::GreatDwarfAlliesStatus::Ready || skillVersion_ == UINT64_MAX ||
            !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        auto own = effect_, remote = effect_;
        bool ownChanged = false, remoteChanged = false;
        if (!own.Advance(57344, c.now, level_, result.owned, ownChanged) ||
            !remote.Advance(peer_.character, c.now, level_, result.peers, remoteChanged))
            return WorldActionResult::Failed;
        // The 26596 row was durably reserved at cast acceptance. Advance
        // announces that exact instance; allocating another would duplicate it.
        if (!c.ReserveSequences(0))
            return WorldActionResult::Failed;
        bool prepared = false;
        std::vector<unsigned> allocated;
        const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
            if (prepared || ids.size() != plan.allies.size())
                return false;
            for (unsigned id : ids)
                if (!id || id >= modern::kFirstConditionInstance)
                    return false;
            effects = nextHammers.Text(own.Text(), nextEnergy, c);
            prepared = true;
            return true;
        };
        const auto committed =
            c.persistPreparedAlliedEffects(RechargeText(rechargeEndsUtc_), prepareOwn, plan.allies, allocated);
        if (committed == WorldConditionCommit::RecoveryRequired) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (committed != WorldConditionCommit::Applied)
            return Done(out);
        if (!prepared) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        effect_ = std::move(own);
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++skillVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = skillVersion_;
        greatDwarfAt_ = greatDwarfUtc_ = 0;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult AdvanceDragonRelease(const WorldActionContext& c, WorldActionOutput& out, bool& handled) {
        handled = false;
        if (!dragonRelease_.Pending())
            return WorldActionResult::Ignored;
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        if (health <= 0 || legends_.Active() != Facets::Legend || dragonReleaseWater_ != (medium_ == 1)) {
            dragonRelease_.Cancel();
            return WorldActionResult::Ignored;
        }
        if (!dragonRelease_.Due(c.now))
            return WorldActionResult::Ignored;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (!nextHammers.Advance(c, legends_.Active(), level_, nextEnergy, result, changed)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        std::vector<WorldConditionActor> allies;
        if (!c.conditionAllies || !c.conditionAllies(allies)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        modern::DragonReleasePlan plan;
        const auto status = dragonRelease_.Prepare(c, allies, plan);
        if (status == modern::DragonReleaseStatus::None)
            return WorldActionResult::Ignored;
        if (status == modern::DragonReleaseStatus::Expired) {
            dragonRelease_.Cancel();
            return WorldActionResult::Ignored;
        }
        handled = true;
        if (status != modern::DragonReleaseStatus::Ready || skillVersion_ == UINT64_MAX ||
            !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        auto own = effect_, remote = effect_;
        bool ownChanged = false, remoteChanged = false;
        if (!own.Advance(57344, c.now, level_, result.owned, ownChanged) ||
            !remote.Advance(peer_.character, c.now, level_, result.peers, remoteChanged))
            return WorldActionResult::Failed;
        for (auto grant : plan.own) {
            if (grant.definition == 718)
                grant.sourceLevel = level_;
            unsigned id = 0, peerId = 0;
            if (!own.ReceiveProtection(57344, grant, c, level_, id, result.owned) ||
                !remote.ReceiveProtection(peer_.character, grant, c, level_, peerId, result.peers) || id != peerId)
                return WorldActionResult::Failed;
        }
        if (!c.ReserveSequences(0))
            return WorldActionResult::Failed;
        bool prepared = false;
        std::vector<unsigned> allocated;
        const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
            if (prepared || ids.size() != plan.allies.size())
                return false;
            for (unsigned id : ids)
                if (!id || id >= modern::kFirstConditionInstance)
                    return false;
            effects = nextHammers.Text(own.Text(), nextEnergy, c);
            prepared = true;
            return true;
        };
        const auto committed =
            c.persistPreparedAlliedEffects(RechargeText(rechargeEndsUtc_), prepareOwn, plan.allies, allocated);
        if (committed == WorldConditionCommit::RecoveryRequired) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (committed != WorldConditionCommit::Applied)
            return Done(out);
        if (!prepared) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        effect_ = std::move(own);
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++skillVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = skillVersion_;
        dragonRelease_.Cancel();
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult AdvanceFacetPulse(const WorldActionContext& c, WorldActionOutput& out, bool& handled) {
        handled = false;
        if (!hammers_.IsFacetMode())
            return WorldActionResult::Ignored;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        WorldActionOutput result;
        result.sequences = 0;
        bool changed = false;
        if (!nextHammers.Advance(c, legends_.Active(), level_, nextEnergy, result, changed)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        std::vector<WorldConditionActor> allies;
        if (!c.conditionAllies || !c.conditionAllies(allies)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        modern::FacetPulsePlan plan;
        const auto status = nextHammers.PrepareFacetPulse(c, allies, plan);
        if (status == modern::FacetPulseStatus::None)
            return WorldActionResult::Ignored;
        handled = true;
        if (status != modern::FacetPulseStatus::Ready || skillVersion_ == UINT64_MAX || !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        std::vector<unsigned> ownInstances;
        auto own = effect_, remote = effect_;
        bool ownChanged = false, remoteChanged = false;
        if (!own.Advance(57344, c.now, level_, result.owned, ownChanged) ||
            !remote.Advance(peer_.character, c.now, level_, result.peers, remoteChanged))
            return WorldActionResult::Failed;
        for (auto grant : plan.own) {
            if (grant.definition == 718)
                grant.sourceLevel = level_;
            unsigned id = 0, peerId = 0;
            if (!own.ReceiveProtection(57344, grant, c, level_, id, result.owned) ||
                !remote.ReceiveProtection(peer_.character, grant, c, level_, peerId, result.peers) || id != peerId)
                return WorldActionResult::Failed;
            ownInstances.push_back(id);
        }
        if (!c.ReserveSequences(0))
            return WorldActionResult::Failed;
        bool prepared = false;
        std::vector<unsigned> allocated;
        const auto prepareOwn = [&](const std::vector<unsigned>& ids, std::string& effects) {
            if (prepared || !nextHammers.CommitFacetPulse(plan, ownInstances, ids))
                return false;
            effects = nextHammers.Text(own.Text(), nextEnergy, c);
            prepared = true;
            return true;
        };
        const auto committed =
            c.persistPreparedAlliedEffects(RechargeText(rechargeEndsUtc_), prepareOwn, plan.allies, allocated);
        if (committed == WorldConditionCommit::RecoveryRequired) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (committed != WorldConditionCommit::Applied)
            return Done(out);
        if (!prepared) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        effect_ = std::move(own);
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++skillVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult AdvanceInspiring(const WorldActionContext& c, WorldActionOutput& out, bool& handled) {
        handled = false;
        if (!inspiring_.Active())
            return WorldActionResult::Ignored;
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        if (health <= 0 || legends_.Active() != 16) {
            inspiring_.Cancel();
            return WorldActionResult::Ignored;
        }
        std::vector<WorldConditionActor> allies;
        if (!c.conditionAllies || !c.conditionAllies(allies)) {
            handled = true;
            return WorldActionResult::Failed;
        }
        modern::InspiringFieldPlan plan;
        const auto status = inspiring_.Prepare(c, c.selfIdentity, allies, plan);
        if (status == modern::InspiringFieldStatus::None)
            return WorldActionResult::Ignored;
        if (status == modern::InspiringFieldStatus::Expired) {
            inspiring_.Cancel();
            return WorldActionResult::Ignored;
        }
        handled = true;
        if (status != modern::InspiringFieldStatus::Ready || skillVersion_ == UINT64_MAX || !c.persistAlliedEffects)
            return WorldActionResult::Failed;
        auto own = effect_, remote = effect_;
        WorldActionOutput result;
        result.sequences = 0;
        unsigned instance = 0, remoteInstance = 0;
        std::string effects = c.skillEffects;
        if (plan.own) {
            if (!own.ReceiveProtection(57344, plan.ownEffect, c, level_, instance, result.owned) ||
                !remote.ReceiveProtection(peer_.character, plan.ownEffect, c, level_, remoteInstance, result.peers) ||
                instance != remoteInstance)
                return WorldActionResult::Failed;
            std::string prefix;
            if (effects.compare(0, 3, "H1:") == 0 || effects.compare(0, 3, "H2:") == 0 ||
                effects.compare(0, 3, "H3:") == 0) {
                const auto bar = effects.find('|');
                if (bar == std::string::npos)
                    return WorldActionResult::Failed;
                prefix = effects.substr(0, bar + 1);
            }
            effects = prefix + own.Text();
        }
        if (!c.ReserveSequences(0))
            return WorldActionResult::Failed;
        std::vector<unsigned> allocated;
        const auto committed = c.persistAlliedEffects(RechargeText(rechargeEndsUtc_), effects, plan.allies, allocated);
        if (committed == WorldConditionCommit::RecoveryRequired) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (committed != WorldConditionCommit::Applied)
            return Done(out);
        if (!inspiring_.Commit(plan, allocated, instance)) {
            absorptionRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (plan.own) {
            effect_ = std::move(own);
            effectsNeedSave_ = false;
            result.peerEffectVersion = ++skillVersion_;
        }
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    modern::LegendActions<167409>::Persist LegendPersistence(const WorldActionContext& context) {
        return [this, &context](const std::string& legends, unsigned active, modern::Energy<167409>& energy,
                                WorldActionOutput& result) {
            if (!hammers_.Tracked()) {
                if (!context.persistLegendState || !context.persistLegendState(legends))
                    return false;
                if (active != 15)
                    CancelAssassinInitialHeal();
                if (active != Facets::Legend)
                    dragonRelease_.Cancel();
                if (active != modern::GreatDwarfData<167409>::Legend)
                    greatDwarfAt_ = greatDwarfUtc_ = 0;
                if (active != Absorption::Legend())
                    absorptionAt_ = absorptionUtc_ = 0;
                return true;
            }
            auto next = hammers_;
            auto nextEffect = effect_;
            if (active != Facets::Legend)
                nextEffect.CancelPending(context.now, modern::BurstStrength::Buff);
            if (active != Maintained::LegendFor(next.CurrentSkill()) && !next.Stop(context, energy, result))
                return false;
            if (!context.persistLegendSkillState ||
                !context.persistLegendSkillState(legends, RechargeText(rechargeEndsUtc_),
                                                 next.Text(nextEffect.Text(), energy, context)))
                return false;
            next.Saved();
            hammers_ = std::move(next);
            effect_ = std::move(nextEffect);
            effectsNeedSave_ = false;
            if (active != 15)
                CancelAssassinInitialHeal();
            if (active != Facets::Legend)
                dragonRelease_.Cancel();
            if (active != modern::GreatDwarfData<167409>::Legend)
                greatDwarfAt_ = greatDwarfUtc_ = 0;
            if (active != Absorption::Legend())
                absorptionAt_ = absorptionUtc_ = 0;
            return true;
        };
    }
    bool MissileAvailable(uint32_t id) const {
        if (id == modern::StealthRules::SelfSource) {
            for (const auto& skill : skills_)
                if (skill.skill == id)
                    return !skill.requiresWeapon && medium_ != 1 && level_ >= skill.minimumLevel;
            return false;
        }
        for (const auto& skill : skills_)
            if (skill.skill == id)
                return skill.underwater == (medium_ == 1) && skill.equipmentSlot < definitions_.size() &&
                       state_.locations[skill.equipmentSlot] == -1 && definitions_[skill.equipmentSlot] == skill.weapon;
        return false;
    }
    bool CancelStance(const WorldActionContext& context, WorldActionOutput& result) {
        if (!effect_.ActiveStance(context.now))
            return true;
        auto next = effect_, peer = effect_;
        if (!next.Consume(57344, result.owned) || !peer.Consume(peer_.character, result.peers) ||
            !context.ReserveSequences(result.sequences) || !context.persistSkillState ||
            !context.persistSkillState(RechargeText(rechargeEndsUtc_), hammers_.Text(next.Text(), energy_, context)))
            return false;
        effect_ = std::move(next);
        effectsNeedSave_ = false;
        return true;
    }
    WorldActionResult SelectAttunement(uint32_t skill, uint8_t slot, uint32_t identity,
                                       const WorldActionContext& context, WorldActionOutput& out) {
        const auto selected = static_cast<uint8_t>(slot - 11);
        if (!attunement_ || selected < 1 || selected > 4 || skill != 5491u + selected ||
            level_ < AttunementLevel(selected))
            return Done(out);
        if (identity == lastInstantIdentity_ && hasInstantIdentity_)
            return Done(out);
        if (skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        const bool changed = selected != attunement_;
        WorldActionOutput result;
        result.sequences = 1;
        if (changed && castEndsAt_ && (!phase_.Active() || phase_.index + 1 < phase_.phases.size())) {
            if (!modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers))
                return WorldActionResult::Failed;
            ++result.sequences;
        }
        if (changed && pendingRecharge_ && !PendingRecharge(context, result.owned))
            return WorldActionResult::Failed;
        if (!AttunementSelection(57344, selected, result.owned) ||
            !AttunementSelection(peer_.character, selected, result.peers) ||
            !SkillBegin(57344, context.sequence + result.sequences - 1, identity, skill, slot, 0, 0, result.owned) ||
            !SkillBegin(peer_.character, context.sequence + result.sequences - 1, identity, skill, slot, 0, 0,
                        result.peers))
            return WorldActionResult::Failed;
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (changed && (!context.persistProfessionState || !context.persistProfessionState(selected)))
            return Done(out);
        if (changed) {
            phase_ = modern::PhasedSkill{};
            castEndsAt_ = 0;
            pendingRecharge_ = false;
            missiles_.CancelPending();
            brand_.CancelWarp();
            stealth_.CancelPending();
            healCast_ = false;
            modern::CancelPendingHeals(pendingHeals_, context.now);
            pendingHeals_.erase(std::remove_if(pendingHeals_.begin(), pendingHeals_.end(),
                                               [](const modern::PendingHeal& heal) {
                                                   return heal.skill == 13027 || heal.skill == 5834 ||
                                                          (heal.skill == 5503 || heal.skill == 13050);
                                               }),
                                pendingHeals_.end());
        }
        attunement_ = selected;
        attunementReported_ = true;
        lastInstantIdentity_ = identity;
        hasInstantIdentity_ = true;
        result.peerEffectVersion = ++skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult MotionMedium(Reader& reader, const WorldActionContext& context, WorldActionOutput& out) {
        uint8_t medium = 0;
        bool valid = false;
        if (!modern::ReadMotionMedium(reader, medium, valid))
            return WorldActionResult::Failed;
        if (!valid || (mediumReported_ && medium == medium_))
            return WorldActionResult::Ignored;
        if (context.peerAgent != peer_.character || !context.sequence || context.sequence == UINT32_MAX ||
            skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        auto nextLegends = legends_;
        if (!nextLegends.Medium(medium == 1, context, result))
            return WorldActionResult::Failed;
        if (medium == medium_) {
            if (!modern::WeaponSelection(284, 57344, medium, result.owned) ||
                !modern::WeaponSelection(284, peer_.character, medium, result.peers))
                return WorldActionResult::Failed;
            if (medium != 1 && !CancelStance(context, result))
                return WorldActionResult::Failed;
            legends_ = std::move(nextLegends);
            mediumReported_ = true;
            result.peerEffectVersion = ++skillVersion_;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (castEndsAt_ && (!phase_.Active() || phase_.index + 1 < phase_.phases.size())) {
            if (!modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
        }
        if (pendingRecharge_) {
            const auto& skill = skills_[pendingRechargeIndex_];
            const uint64_t end = rechargeEndsAt_[pendingRechargeIndex_];
            const auto remaining = static_cast<unsigned>(
                (std::min)(end > context.now ? end - context.now : 0, uint64_t(skill.rechargeMs)));
            if (!SkillRecharge(skill.skill, remaining, skill.rechargeMs, result.owned))
                return WorldActionResult::Failed;
        }
        if (!modern::WeaponSelection(284, 57344, medium, result.owned) ||
            !modern::WeaponSelection(284, peer_.character, medium, result.peers))
            return WorldActionResult::Failed;
        if (medium != 1 && !CancelStance(context, result))
            return WorldActionResult::Failed;
        auto nextEffect = effect_;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        // The first observed medium after reconnect must not cancel a restored Rite.
        const bool changedMedium = mediumReported_ || castEndsAt_;
        const bool cancelledEffect = changedMedium && nextEffect.CancelPending(context.now);
        const bool cancelledHammers = changedMedium && nextHammers.Present();
        if (cancelledHammers && !nextHammers.Stop(context, nextEnergy, result))
            return WorldActionResult::Failed;
        if (cancelledEffect || cancelledHammers) {
            if (!context.ReserveSequences(result.sequences) || !context.persistSkillState ||
                !context.persistSkillState(RechargeText(rechargeEndsUtc_),
                                           nextHammers.Text(nextEffect.Text(), nextEnergy, context)))
                return WorldActionResult::Failed;
            effect_ = std::move(nextEffect);
            effectsNeedSave_ = false;
            nextHammers.Saved();
            hammers_ = std::move(nextHammers);
            energy_ = std::move(nextEnergy);
        }
        phase_ = modern::PhasedSkill{};
        castEndsAt_ = 0;
        pendingRecharge_ = false;
        missiles_.CancelPending();
        brand_.CancelWarp();
        stealth_.CancelPending();
        healCast_ = false;
        modern::CancelPendingHeals(pendingHeals_, context.now);
        pendingHeals_.erase(std::remove_if(pendingHeals_.begin(), pendingHeals_.end(),
                                           [](const modern::PendingHeal& heal) {
                                               return heal.skill == 13027 || heal.skill == 5834 ||
                                                      (heal.skill == 5503 || heal.skill == 13050);
                                           }),
                            pendingHeals_.end());
        absorptionAt_ = absorptionUtc_ = 0;
        approachAt_ = approachUtc_ = 0;
        backstepAt_ = backstepUtc_ = 0;
        greatDwarfAt_ = greatDwarfUtc_ = 0;
        dragonRelease_.Cancel();
        CancelAssassinInitialHeal();
        legends_ = std::move(nextLegends);
        medium_ = medium;
        mediumReported_ = true;
        result.peerEffectVersion = ++skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool RestoreRecharges(const WorldActionContext& context) {
        if (rechargesRestored_)
            return hammers_.ClockValid(context);
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        if (!nextHammers.Restore(context, nextEnergy) || !nextHammers.ClockValid(context))
            return false;
        std::vector<uint64_t> restored(skills_.size(), 0);
        std::vector<bool> reports(skills_.size(), false);
        for (size_t i = 0; i < skills_.size(); ++i)
            if (rechargeEndsUtc_[i]) {
                if (!context.wallNow || context.peerAgent != peer_.character)
                    return false;
                const uint64_t remaining =
                    rechargeEndsUtc_[i] > context.wallNow ? rechargeEndsUtc_[i] - context.wallNow : 0;
                const auto id = skills_[i].skill;
                const unsigned activation =
                    id == 26679 ? 650
                    : Maintained::IsEliteSkill(id)
                        ? (std::max)(Maintained::ActivationFor(id, false), Maintained::ActivationFor(id, true))
                        : skills_[i].activationMs;
                const uint64_t maximum = static_cast<uint64_t>(activation) + skills_[i].rechargeMs;
                if (remaining > maximum || context.now > UINT64_MAX - remaining)
                    return false;
                restored[i] = remaining ? context.now + remaining : 0;
                reports[i] = true;
            }
        auto effect = effect_;
        if (!effect.Restore(context.now, context.wallNow))
            return false;
        const bool saveRestored = nextHammers.NeedsSave();
        if (saveRestored) {
            if (!context.persistSkillState ||
                !context.persistSkillState(RechargeText(rechargeEndsUtc_),
                                           nextHammers.Text(effect.Text(), nextEnergy, context)))
                return false;
            nextHammers.Saved();
        }
        effectsNeedSave_ = !saveRestored && effect.Text() != effect_.Text();
        effect_ = std::move(effect);
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        rechargeEndsAt_ = std::move(restored);
        resumeReports_ = std::move(reports);
        rechargesRestored_ = true;
        return true;
    }
    std::string RechargeText(const std::vector<uint64_t>& deadlines) const {
        std::string text;
        for (size_t i = 0; i < skills_.size(); ++i)
            if (deadlines[i]) {
                if (!text.empty())
                    text += ',';
                text += std::to_string(skills_[i].skill) + ':' + std::to_string(deadlines[i]);
            }
        return text;
    }
    bool PendingRecharge(const WorldActionContext& context, Buffer& out) const {
        const auto& skill = skills_[pendingRechargeIndex_];
        const uint64_t end = rechargeEndsAt_[pendingRechargeIndex_];
        // A selector can interrupt before activation finishes. The saved deadline
        // includes that activation, but the native recharge display is bounded.
        const unsigned remaining =
            static_cast<unsigned>((std::min)(end > context.now ? end - context.now : 0, uint64_t(skill.rechargeMs)));
        return SkillRecharge(skill.skill, remaining, skill.rechargeMs, out);
    }
    WorldActionResult SkillRequest(Reader& reader, const WorldActionContext& context, WorldActionOutput& out) {
        uint16_t agent = 0;
        uint32_t clientTick = 0, identity = 0;
        uint8_t flags = 0, size = 0;
        ByteView blob;
        if (!reader.U16(agent) || reader.VarU32(clientTick) != DecodeStatus::Complete || !reader.U8(flags) ||
            reader.VarU32(identity) != DecodeStatus::Complete || !reader.U8(size) || !reader.Bytes(size, blob) ||
            reader.Remaining())
            return WorldActionResult::Failed;
        if (!size || (blob.data[0] != 35 && blob.data[0] != 37))
            return WorldActionResult::Ignored;
        const bool ground = blob.data[0] == 37;
        if (size != (ground ? 22 : 10) || context.peerAgent != peer_.character || agent != 57344 || !context.sequence ||
            context.sequence == UINT32_MAX)
            return WorldActionResult::Failed;
        Reader payload(blob);
        uint8_t command = 0, slot = 0;
        uint32_t targetOrFrame = 0, skillId = 0;
        float point[3]{};
        if (!payload.U8(command))
            return WorldActionResult::Failed;
        if (ground) {
            // Native C23 behavior 37: three coordinates, coordinate frame,
            // skill and slot. The fourth point word is not a target agent.
            // The point is in the engine's spatial units, not the
            // catalog units used by movement. Invalid floats consume no cast.
            ByteView pointBytes;
            if (!payload.Bytes(sizeof(point), pointBytes))
                return WorldActionResult::Failed;
            std::memcpy(point, pointBytes.data, sizeof(point));
            for (float coordinate : point)
                if (!std::isfinite(coordinate))
                    return WorldActionResult::Failed;
        }
        if (!payload.U32(targetOrFrame) || !payload.U32(skillId) || !payload.U8(slot) || payload.Remaining())
            return WorldActionResult::Failed;
        const auto stance = StanceForSkill(skillId), followup = StanceForFollowup(skillId);
        const bool brandBase = skillId == modern::RevenantBrandRules::Base,
                   brandFollowup = skillId == modern::RevenantBrandRules::Followup;
        if (brandFollowup && !brand_.Active(context))
            return Done(out);
        if (skillId == modern::StealthRules::Attack && !stealth_.Active(context.now))
            return Done(out);
        const bool selfStealth = skillId == modern::StealthRules::SelfSource, engineerHeal = skillId == 5834,
                   simpleHeal = skillId == 5503 || skillId == 13050;
        const bool venom = skillId == modern::RevenantSphereRules::Base;
        const bool venomFollowup = skillId == modern::RevenantSphereRules::Followup;
        const bool detonate = skillId == 5653 || venomFollowup;
        const uint32_t sphereBase = venomFollowup ? modern::RevenantSphereRules::Base : 5652;
        if (detonate && (!missiles_.CanDetonate(context.now, sphereBase) || (skillId == 5653 && attunement_ != 3)))
            return Done(out);
        if (detonate && skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        const bool dwarfHeal = skillId == DwarfHealingData::Skill;
        const bool demonHeal = skillId == DemonHealingData::Skill, assassinHeal = skillId == 26937,
                   legendHeal = dwarfHeal || demonHeal || assassinHeal;
        const auto facetRule = Facets::RuleFor(skillId, medium_ == 1);
        const bool facet = facetRule.base != 0, facetOn = facet && Facets::IsBase(skillId),
                   facetHeal = skillId == 27228, facetRelease = skillId == DragonRelease::Skill;
        const bool facetBurst = skillId == modern::BurstStrength::Skill;
        const bool facetGaze = skillId == combat::GazeDarkness::Skill,
                   facetElements = skillId == combat::ElementalBlast::Skill;
        // Other consume actions retain their unresolved combat effects.
        if (facet && !facetOn && !facetHeal && !facetRelease && !facetBurst && !facetGaze && !facetElements)
            return Notice("This facet consumption is not available yet.", out);
        if (facet && legends_.Active() != facetRule.legend)
            return Notice("This skill is not available yet.", out);
        if ((facetOn && hammers_.HasFacet(facetRule.base)) ||
            ((facetHeal || facetRelease || facetBurst || facetGaze || facetElements) &&
             !hammers_.FacetActive(facetRule.base)))
            return Done(out);
        // The consume's own cooldown is the durable pair gate; do not invent a second base timer.
        if (facetOn)
            for (size_t i = 0; i < skills_.size(); ++i)
                if (skills_[i].skill == facetRule.alternate && rechargeEndsAt_[i] > context.now)
                    return Done(out);
        const bool maintained = !facet && Maintained::IsMaintainedSkill(skillId);
        const bool maintainedOn = maintained && Maintained::StartFor(skillId) == skillId,
                   maintainedOff = maintained && !maintainedOn;
        const bool elite = Maintained::IsEliteSkill(skillId), assassin = Maintained::IsAssassinSkill(skillId),
                   absorption = skillId == Absorption::Skill, inspiring = skillId == Inspiring::Skill,
                   approach = skillId == Approach::Skill, backstep = skillId == Backstep::Skill,
                   leap = skillId == DemonLeap::Skill;
        if (leap && legends_.Active() != 17)
            return Notice("This skill is not available yet.", out);
        if (maintained && legends_.Active() != Maintained::LegendFor(skillId))
            return Notice("This skill is not available yet.", out);
        if ((maintainedOn && hammers_.Present()) ||
            (maintainedOff &&
             (!hammers_.Active(context.now) || hammers_.CurrentSkill() != Maintained::StartFor(skillId))))
            return Done(out);
        const bool engagement = skillId == 26679, jade = skillId == 28406,
                   banish = skillId == combat::BanishEnchantment::Skill;
        const bool greatDwarf = skillId == modern::GreatDwarfData<167409>::Skill,
                   legendAction = engagement || jade || banish || legendHeal || greatDwarf || maintainedOn ||
                                  maintainedOff || absorption || inspiring || approach || backstep || leap || facet;
        const uint32_t target = (banish || engagement) ? targetOrFrame
                                : approach             ? targetOrFrame
                                : brandFollowup        ? brand_.Target()
                                : ground || selfStealth || engineerHeal || simpleHeal || skillId == kAirAuraSkill ||
                                        stance || followup || detonate || venom || legendAction
                                    ? 0
                                    : targetOrFrame;
        if (!ground && slot >= 12 && slot <= 15) {
            if (legends_.Enabled())
                return legends_.Swap(skillId, slot, targetOrFrame, identity, context, level_, energy_, skillVersion_,
                                     out, LegendPersistence(context));
            return SelectAttunement(skillId, slot, identity, context, out);
        }
        const InitialWeaponSkill* skill = nullptr;
        size_t skillIndex = 0;
        for (; skillIndex < skills_.size(); ++skillIndex) {
            const auto& candidate = skills_[skillIndex];
            const unsigned nativeSlot = candidate.nativeSlot == 255 ? candidate.slotType + 2 : candidate.nativeSlot;
            if (candidate.skill == skillId && nativeSlot == slot) {
                skill = &candidate;
                break;
            }
        }
        InitialWeaponSkill ownAssassinHeal;
        if (assassinHeal && skill) {
            ownAssassinHeal = *skill;
            ConfigureAssassinInitialHeal(ownAssassinHeal, medium_ == 1);
            skill = &ownAssassinHeal;
        }
        InitialWeaponSkill ownFacet;
        if (facet && skill) {
            ownFacet = *skill;
            ownFacet.activationMs = facetRule.activation;
            ownFacet.animationMs = facetRule.animation;
            ownFacet.recoveryMs = facetRule.animation - facetRule.activation;
            ownFacet.rechargeMs = facetRule.recharge;
            ownFacet.variableStages = false;
            ownFacet.phases.clear();
            ownFacet.underwater = false;
            ownFacet.groundTargeted = false;
            ownFacet.minimumGroundRange = ownFacet.maximumGroundRange = 0;
            skill = &ownFacet;
        }
        InitialWeaponSkill ownMaintained;
        if ((elite || assassin) && skill) {
            // Own land/water phases; off removes state immediately by local policy.
            // The recorded off trigger is not treated as a removal deadline.
            ownMaintained = *skill;
            ownMaintained.activationMs = Maintained::ActivationFor(skillId, medium_ == 1);
            ownMaintained.animationMs = Maintained::AnimationFor(skillId, medium_ == 1);
            ownMaintained.recoveryMs = ownMaintained.animationMs - ownMaintained.activationMs;
            ownMaintained.rechargeMs = Maintained::RechargeFor(skillId);
            ownMaintained.variableStages = false;
            ownMaintained.phases.clear();
            skill = &ownMaintained;
        }
        InitialWeaponSkill ownAbsorption;
        if (absorption && skill) {
            ownAbsorption = *skill;
            ownAbsorption.activationMs = Absorption::ActivationMs(medium_ == 1);
            ownAbsorption.animationMs = Absorption::AnimationMs(medium_ == 1);
            ownAbsorption.recoveryMs = ownAbsorption.animationMs - ownAbsorption.activationMs;
            ownAbsorption.rechargeMs = 0;
            ownAbsorption.variableStages = false;
            ownAbsorption.phases.clear();
            skill = &ownAbsorption;
        }
        InitialWeaponSkill ownApproach;
        if (approach && skill) {
            ownApproach = *skill;
            ConfigureApproach(ownApproach, medium_ == 1);
            skill = &ownApproach;
        }
        InitialWeaponSkill ownLeap;
        if (leap && skill) {
            ownLeap = *skill;
            ConfigureDemonLeap(ownLeap, medium_ == 1);
            skill = &ownLeap;
        }
        InitialWeaponSkill ownBackstep;
        if (backstep && skill) {
            ownBackstep = *skill;
            ConfigureBackstep(ownBackstep, medium_ == 1);
            skill = &ownBackstep;
        }
        InitialWeaponSkill ownJade;
        if (skillId == 28406 && skill) {
            ownJade = *skill;
            ownJade.activationMs = combat::JadeWinds::Activation(167409, medium_ == 1);
            ownJade.animationMs = 2033;
            ownJade.recoveryMs = 2033 - ownJade.activationMs;
            skill = &ownJade;
        }
        InitialWeaponSkill ownEngagement;
        if (skillId == 26679 && skill) {
            ownEngagement = *skill;
            ownEngagement.activationMs = combat::ForcedEngagement::Activation(167409, medium_ == 1);
            ownEngagement.animationMs = combat::ForcedEngagement::Animation(167409, medium_ == 1);
            ownEngagement.recoveryMs = ownEngagement.animationMs - ownEngagement.activationMs;
            ownEngagement.minimumGroundRange = 0;
            ownEngagement.maximumGroundRange = 900;
            ownEngagement.phases.clear();
            ownEngagement.variableStages = medium_ != 1;
            if (medium_ != 1) {
                modern::SkillStage first;
                first.durationMs = 500;
                ownEngagement.phases.push_back(first);
                modern::SkillStage second;
                second.index = 1;
                second.durationMs = 733;
                ownEngagement.phases.push_back(second);
            }
            skill = &ownEngagement;
        }
        InitialWeaponSkill ownInspiring;
        if (inspiring && skill) {
            ownInspiring = *skill;
            ownInspiring.activationMs = 400;
            ownInspiring.animationMs = 1233;
            ownInspiring.recoveryMs = 833;
            ownInspiring.rechargeMs = 10000;
            ownInspiring.variableStages = false;
            ownInspiring.phases.clear();
            ownInspiring.groundTargeted = true;
            ownInspiring.minimumGroundRange = 0;
            ownInspiring.maximumGroundRange = 900;
            skill = &ownInspiring;
        }
        if (!skill || (skill->requiresWeapon &&
                       (skill->equipmentSlot >= definitions_.size() || state_.locations[skill->equipmentSlot] != -1 ||
                        definitions_[skill->equipmentSlot] != skill->weapon)))
            return Notice("That weapon skill is not available with the current equipment.", out);
        if (skill->requiresWeapon && skill->underwater != (medium_ == 1))
            return Notice("That skill is not available in this environment.", out);
        if (selfStealth && (medium_ == 1 || level_ < skill->minimumLevel))
            return Notice("This skill is not available in this environment.", out);
        if (engineerHeal && (medium_ != 1 || level_ < skill->minimumLevel))
            return Notice("This skill is only available underwater.", out);
        if (skillId == 13050 && medium_ != 1)
            return Notice("This skill is only available underwater.", out);
        if (simpleHeal && level_ < skill->minimumLevel)
            return Notice("This skill is not available yet.", out);
        if (!skill->requiresWeapon && !selfStealth && !engineerHeal && !simpleHeal &&
            (!legendAction || level_ < skill->minimumLevel ||
             legends_.Active() != (engagement               ? combat::ForcedEngagement::Legend(167409)
                                   : jade                   ? combat::JadeWinds::Legend(167409)
                                   : banish                 ? combat::BanishEnchantment::Legend(167409)
                                   : facet                  ? facetRule.legend
                                   : leap                   ? 17
                                   : (approach || backstep) ? 15
                                   : maintained             ? Maintained::LegendFor(skillId)
                                   : absorption             ? Absorption::Legend()
                                   : assassinHeal           ? 15
                                   : demonHeal              ? DemonHealingData::Legend
                                                            : DwarfHealingData::Legend)))
            return Notice("This skill is not available yet.", out);
        if (followup && effect_.ActiveBuff(context.now) != followup->buff)
            return Done(out);
        if (stance && effect_.ActiveBuff(context.now) == stance->buff)
            return Done(out);
        if (banish || engagement) {
            WorldCombatTarget enemy;
            if (!target || !context.combatTarget || !context.combatTarget(target, enemy))
                return Notice("Choose a combat opponent.", out);
            const auto& p = enemy.actor.position;
            const double distance =
                32 * std::hypot(std::hypot(double(p.x) - context.position.x, double(p.y) - context.position.y),
                                double(p.z) - context.position.z);
            if (!std::isfinite(distance) || distance > (engagement ? combat::ForcedEngagement::Range(167409)
                                                                   : combat::BanishEnchantment::Range(167409)))
                return Notice("That opponent is out of range.", out);
        }
        if (skill->groundTargeted != ground)
            return Notice("Choose the correct target for this skill.", out);
        if (ground) {
            // Native spatial points can use local moving-object frames. Only
            // frame 0 is a world point; never treat another frame as an agent.
            if (targetOrFrame)
                return Notice("Choose a point on the ground.", out);
            const float origin[3] = {context.position.x * 32, context.position.y * 32, -context.position.z * 32};
            for (float coordinate : origin)
                if (!std::isfinite(coordinate))
                    return WorldActionResult::Failed;
            const float x = point[0] - origin[0], y = point[1] - origin[1], z = point[2] - origin[2];
            const float distanceSquared = x * x + y * y + z * z;
            // Original ground selection accepts min <= distance <= max.
            // Reuse movement's world conversion, including the vertical sign.
            if (!std::isfinite(distanceSquared) ||
                distanceSquared < skill->minimumGroundRange * skill->minimumGroundRange ||
                distanceSquared > skill->maximumGroundRange * skill->maximumGroundRange)
                return Notice("That point is outside this skill's range.", out);
        }
        if (target && (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(target)))
            return Notice("That target is no longer in this map.", out);
        if ((skill->underwater || skill->maximumGroundRange > 0) &&
            !modern::SkillTargetInRange(context, target, skill->minimumGroundRange, skill->maximumGroundRange))
            return Notice("That target is outside this skill's range.", out);
        CatalogPosition approachDestination;
        bool approachRelocate = false;
        if (approach && (!Approach::Destination(context, target, medium_ == 1, approachDestination, approachRelocate) ||
                         (approachRelocate && approachMotion_.empty())))
            return Notice("Move once and choose another visible target within range.", out);
        const CatalogPosition leapDestination{point[0] / 32, point[1] / 32, -point[2] / 32};
        if (leap && !DemonLeap::ValidPoint(context, leapDestination))
            return Done(out);
        const bool instant = skill->animationMs == 0;
        // Do not overwrite a due transfer whose atomic write still needs retry.
        if ((absorptionAt_ || approachAt_ || backstepAt_) && !instant)
            return Done(out);
        if (backstep && (!context.wallNow || context.wallNow > UINT64_MAX - Backstep::Activation))
            return WorldActionResult::Failed;
        if (approach && (!context.wallNow || context.wallNow > UINT64_MAX - Approach::Activation - Approach::Duration ||
                         context.now > UINT64_MAX - Approach::Activation - Approach::Duration))
            return WorldActionResult::Failed;
        if (absorption && (!context.wallNow || context.wallNow > UINT64_MAX - Absorption::ActivationMs(medium_ == 1)))
            return WorldActionResult::Failed;
        const bool phased = skill->variableStages || (skill->underwater && !instant);
        if (instant ? (identity == lastInstantIdentity_ && hasInstantIdentity_)
                    : (identity == lastSkillIdentity_ && hasSkillIdentity_))
            return Done(out);
        const bool interruptMaintained = elite && maintainedOff && castEndsAt_ && context.now < castEndsAt_ &&
                                         maintainedCast_ == Maintained::StartFor(skillId);
        const bool interrupt =
            (context.taunted && backstep && (phase_.Active() || castEndsAt_)) || interruptMaintained ||
            (phase_.Active() && ((followup && phase_.skill == followup->skill) ||
                                 (venomFollowup && phase_.skill == modern::RevenantSphereRules::Base) ||
                                 (brandFollowup && phase_.skill == modern::RevenantBrandRules::Base)));
        if (!instant && !interrupt && (phase_.Active() || (castEndsAt_ && context.now < castEndsAt_)))
            return Done(out);
        if (rechargeEndsAt_[skillIndex] && context.now < rechargeEndsAt_[skillIndex])
            return Done(out);
        if (!energy_.CanSpend(skillId, context.now))
            return Notice("Not enough energy for this skill.", out);
        if ((legendAction || selfStealth || engineerHeal || simpleHeal) && !maintainedOff) {
            float health = 0;
            if (!modern::ReadHealthState(context.healthState, health) ||
                ((legendHeal || selfStealth || engineerHeal || simpleHeal) && pendingHeals_.size() >= 64))
                return WorldActionResult::Failed;
            if (health == 0)
                return Notice("This skill cannot be used while defeated.", out);
        }
        const uint64_t duration = static_cast<uint64_t>(skill->activationMs) + skill->recoveryMs;
        const uint64_t rechargeDuration = static_cast<uint64_t>(skill->activationMs) + skill->rechargeMs;
        if (context.now > UINT64_MAX - duration || context.now > UINT64_MAX - rechargeDuration)
            return WorldActionResult::Failed;
        std::vector<uint64_t> savedRecharges = rechargeEndsUtc_;
        if (skill->rechargeMs) {
            if (!context.wallNow || context.wallNow > UINT64_MAX - rechargeDuration)
                return WorldActionResult::Failed;
            for (auto& end : savedRecharges)
                if (end <= context.wallNow)
                    end = 0;
            savedRecharges[skillIndex] = context.wallNow + rechargeDuration;
        }
        if (greatDwarf && greatDwarfAt_ && context.wallNow < greatDwarfUtc_ + RiteAllies::Duration)
            return Done(out);
        if (greatDwarf &&
            (!context.wallNow || context.now > UINT64_MAX - RiteAllies::Activation() - RiteAllies::Duration ||
             context.wallNow > UINT64_MAX - RiteAllies::Activation() - RiteAllies::Duration))
            return WorldActionResult::Failed;
        auto nextRelease = dragonRelease_;
        if (facetRelease && !nextRelease.Schedule(context, medium_ == 1))
            return Done(out);
        auto nextInspiring = inspiring_;
        if (inspiring && (!context.wallNow || context.wallNow > UINT64_MAX - 400 ||
                          !nextInspiring.Begin(context.position, {point[0] / 32, point[1] / 32, -point[2] / 32},
                                               context.now + 400, context.wallNow + 400)))
            return Done(out);
        WorldActionOutput result;
        result.sequences = interrupt ? 2 : 1;
        if (interrupt && context.sequence >= UINT32_MAX - 1)
            return WorldActionResult::Failed;
        if (interrupt && (!modern::SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                          !modern::SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers)))
            return WorldActionResult::Failed;
        const uint32_t startSequence = context.sequence + (interrupt ? 1 : 0);
        modern::PhasedSkill nextPhase;
        if (phased) {
            if (skillVersion_ == UINT64_MAX)
                return WorldActionResult::Failed;
            if (target == 57344)
                return Notice("Choose another target for this skill.", out);
            if (!nextPhase.Begin(skill->phases, static_cast<unsigned>(duration), skillId, slot, identity, target,
                                 skill->underwater, skill->minimumGroundRange,
                                 (skill->underwater || skill->maximumGroundRange > 0) ? skill->maximumGroundRange
                                                                                      : -1) ||
                !nextPhase.Emit(approach ? ApproachPhaseContext(context) : context, startSequence, result, true))
                return WorldActionResult::Failed;
        }
        // The behavior prefix identifies the client's predicted order. Native
        // BeginOrder and CompleteOrder match it exactly, including the high bit.
        // It is independent of the client tick and the server's deadline clock.
        if (!instant && pendingRecharge_ && !PendingRecharge(context, result.owned))
            return WorldActionResult::Failed;
        if ((legendAction || selfStealth || engineerHeal || simpleHeal) && !approach && !backstep && !leap &&
            !instant) {
            // Keep the one-step aquatic animation; never replay terrestrial step 1.
            modern::SkillStage stage;
            stage.index = 0;
            stage.durationMs = static_cast<uint16_t>(elite || absorption || inspiring || facet || selfStealth ||
                                                             engineerHeal || simpleHeal
                                                         ? skill->animationMs
                                                         : skill->activationMs);
            if (!modern::SkillPhase(57344, startSequence, identity, skillId, slot, 0, stage, true, false, nullptr,
                                    result.owned, medium_ == 1) ||
                !modern::SkillPhase(peer_.character, startSequence, identity, skillId, slot, 0, stage, true, false,
                                    nullptr, result.peers, medium_ == 1))
                return WorldActionResult::Failed;
        } else if (!phased && (!SkillBegin(57344, startSequence, identity, skillId, slot, skill->activationMs, target,
                                           result.owned) ||
                               !SkillBegin(peer_.character, startSequence, identity, skillId, slot, skill->activationMs,
                                           target, result.peers)))
            return WorldActionResult::Failed;
        if (target && !phased) {
            result.targetPeerAgent = static_cast<uint16_t>(target);
            if (!SkillBegin(peer_.character, context.sequence, identity, skillId, slot, skill->activationMs, 57344,
                            result.targetView))
                return WorldActionResult::Failed;
        }
        if (instant && skill->rechargeMs && !SkillRecharge(skillId, skill->rechargeMs, skill->rechargeMs, result.owned))
            return WorldActionResult::Failed;
        const bool applyEffect = !legendAction && SelfEffectRules::SkillDuration(skillId) != 0;
        auto nextMissiles = missiles_;
        auto nextBrand = brand_;
        auto nextStealth = stealth_;
        auto nextEnergy = energy_;
        if (skill->requiresWeapon &&
            !nextStealth.Use(skillId, target, skill->activationMs, skill->maximumGroundRange, level_, context))
            return WorldActionResult::Failed;
        if (selfStealth && !nextStealth.ScheduleSelf(skill->activationMs, level_, context))
            return WorldActionResult::Failed;
        if (brandBase && !nextBrand.Schedule(target, level_, context))
            return Notice("Move once and choose a visible target in range.", out);
        if (brandFollowup && !nextBrand.Consume(context, result))
            return Done(out);
        modern::ControlledMissileGroup projectile;
        modern::ControlledMissileGroup sphere;
        sphere.marker = modern::RevenantSphereRules::Marker;
        sphere.instance = modern::RevenantSphereRules::Instance;
        sphere.appearanceSkill = modern::RevenantSphereRules::Appearance;
        sphere.speed = modern::RevenantSphereRules::Speed;
        if (!nextMissiles.Schedule(skillId,
                                   skillId == modern::StealthRules::Source ? modern::StealthRules::ReleaseMs
                                   : brandBase                             ? modern::RevenantBrandRules::ReleaseMs
                                   : venom                                 ? modern::RevenantSphereRules::ReleaseMs
                                                                           : skill->activationMs,
                                   skill->maximumGroundRange, target, context, level_, 167409,
                                   brandBase ? &projectile
                                   : venom   ? &sphere
                                             : nullptr) ||
            (detonate && !nextMissiles.Detonate(167409, context, result, sphereBase)))
            return WorldActionResult::Failed;
        const bool saveEffect = applyEffect || followup || greatDwarf || maintainedOn || maintainedOff || absorption ||
                                inspiring || approach || backstep || leap || facet || hammers_.Tracked();
        if (saveEffect && skillVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        auto nextEffect = effect_, peerEffect = effect_;
        auto nextHammers = hammers_;
        if (greatDwarf && (skillVersion_ == UINT64_MAX || !nextEffect.Schedule(skillId, context.now, context.wallNow)))
            return WorldActionResult::Failed;
        if (facetBurst && !nextEffect.Schedule(skillId, context.now, context.wallNow, medium_ == 1))
            return WorldActionResult::Failed;
        if (applyEffect) {
            if (skillVersion_ == UINT64_MAX ||
                !nextEffect.Start(57344, skillId, context.now, context.wallNow, level_, result.owned) ||
                !peerEffect.Start(peer_.character, skillId, context.now, context.wallNow, level_, result.peers))
                return WorldActionResult::Failed;
        }
        if (followup && (skillVersion_ == UINT64_MAX || !nextEffect.Consume(57344, result.owned) ||
                         !peerEffect.Consume(peer_.character, result.peers)))
            return WorldActionResult::Failed;
        // Save before publishing the action. Failed storage consumes neither
        // the predicted identity nor the skill, and cannot bypass a recharge.
        bool hammersChanged = false;
        if (!maintainedOff &&
            !nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, result, hammersChanged))
            return WorldActionResult::Failed;
        // The free toggle must preserve the original exhaustion instant.
        if (!maintainedOff && !nextEnergy.Spend(skillId, context.now, result))
            return WorldActionResult::Failed;
        if (facetOn) {
            uint32_t instance = 0;
            if (skillVersion_ == UINT64_MAX || !nextEffect.ReserveInstance(instance) ||
                !nextHammers.ScheduleFacet(facetRule.base, instance, context, nextEnergy, medium_ == 1))
                return WorldActionResult::Failed;
        }
        if (maintainedOn) {
            uint32_t instance = 0;
            if (skillVersion_ == UINT64_MAX || !nextEffect.ReserveInstance(instance) ||
                !nextHammers.Schedule(instance, context, nextEnergy, skillId, medium_ == 1))
                return WorldActionResult::Failed;
        }
        // Zero authored animation: apply the maintained marker in the same
        // candidate transaction as its initial energy cost, before publishing.
        if (assassin && maintainedOn &&
            (!nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, result, hammersChanged) ||
             !nextEnergy.Advance(context.now, result)))
            return WorldActionResult::Failed;
        if ((facetRelease || facetBurst || facetGaze || facetElements) &&
            (skillVersion_ == UINT64_MAX || !nextHammers.ConsumeFacet(facetRule.base, context, nextEnergy, result) ||
             !nextEnergy.Advance(context.now, result)))
            return WorldActionResult::Failed;
        float facetPrevious = 0, facetCurrent = 0;
        std::string facetHealth;
        if (facetHeal) {
            if (!nextEffect.Schedule(combat::InfuseLight::Skill, context.now, context.wallNow, medium_ == 1))
                return WorldActionResult::Failed;
            if (skillVersion_ == UINT64_MAX || !modern::ReadHealthState(context.healthState, facetPrevious) ||
                facetPrevious <= 0 || !nextHammers.ConsumeFacet(facetRule.base, context, nextEnergy, result) ||
                !nextEnergy.Advance(context.now, result))
                return WorldActionResult::Failed;
            // Own healing model, current profile healing attribute6 is zero.
            facetCurrent =
                (std::min)(modern::kPublishedMaximumHealth,
                           facetPrevious + 125.0f + static_cast<float>(std::round(double(level_) * level_ * .27)));
            const float delta = facetCurrent - facetPrevious;
            if (!modern::WriteHealthState(facetCurrent, facetHealth) ||
                (delta > 0 && (!modern::HealthChange<167409>(57344, 27228, delta, result.owned) ||
                               !modern::HealthChange<167409>(peer_.character, 27228, delta, result.peers))))
                return WorldActionResult::Failed;
        }
        if (maintainedOff && (skillVersion_ == UINT64_MAX || !nextHammers.Stop(context, nextEnergy, result)))
            return WorldActionResult::Failed;
        modern::InitiativeLedger initiative;
        const bool initiativeEnabled = bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return WorldActionResult::Failed;
        if (initiativeEnabled) {
            unsigned cost = 0;
            if ((skill->requiresWeapon && !modern::InitialInitiativeCost(167409, skillId, cost)) ||
                !initiative.Load(context.initiativeState) || !initiative.Advance(context.wallNow))
                return WorldActionResult::Failed;
            if (!initiative.Spend(cost))
                return Done(out);
            if (!modern::InitiativeCurrent(167409, 57344, initiative, result.owned))
                return WorldActionResult::Failed;
        }
        if ((initiativeEnabled || skill->rechargeMs || saveEffect) && !context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (initiativeEnabled || saveEffect) {
            const auto effects = nextHammers.Text(nextEffect.Text(), nextEnergy, context);
            const auto hostile =
                facetGaze ? combat::GazeDarkness::Commit(167409, context, RechargeText(savedRecharges), effects)
                          : WorldConditionCommit::Applied;
            if (hostile == WorldConditionCommit::RecoveryRequired)
                return WorldActionResult::Failed;
            const bool stored =
                facetGaze ? hostile == WorldConditionCommit::Applied
                : initiativeEnabled
                    ? context.persistCombatInitiative(RechargeText(savedRecharges), effects, context.healthState,
                                                      context.conditionState, initiative.Text())
                : facetHeal ? (context.persistCombatConditions &&
                               context.persistCombatConditions(RechargeText(savedRecharges), effects, facetHealth,
                                                               context.conditionState))
                            : (backstep && (context.stunned || context.taunted)
                                   ? (context.persistStunBreakState &&
                                      context.persistStunBreakState(RechargeText(savedRecharges), effects))
                                   : (context.persistSkillState &&
                                      context.persistSkillState(RechargeText(savedRecharges), effects)));
            if (!stored)
                return initiativeEnabled ? Done(out)
                       : maintainedOn || maintainedOff || absorption || inspiring || approach || backstep || leap ||
                               facet
                           ? Done(out)
                           : Notice("The skill state could not be saved.", out);
            effect_ = std::move(nextEffect);
            effectsNeedSave_ = false;
        } else if (skill->rechargeMs &&
                   (!context.persistSkillRecharges || !context.persistSkillRecharges(RechargeText(savedRecharges))))
            return Notice("The skill recharge could not be saved.", out);
        rechargeEndsUtc_ = std::move(savedRecharges);
        resumeReports_[skillIndex] = false;
        missiles_ = std::move(nextMissiles);
        brand_ = std::move(nextBrand);
        stealth_ = std::move(nextStealth);
        energy_ = std::move(nextEnergy);
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        if (legendHeal || selfStealth || engineerHeal || simpleHeal) {
            modern::PendingHeal heal;
            heal.startsAt = context.now + skill->activationMs;
            heal.skill = skillId;
            heal.amount = simpleHeal     ? 75 + static_cast<int32_t>(std::round(double(level_) * level_ * .5f))
                          : engineerHeal ? 120 + static_cast<int32_t>(std::round(double(level_) * level_ * .85f))
                          : selfStealth  ? 138 + static_cast<int32_t>(std::round(double(level_) * level_ * .92f))
                          : assassinHeal ? 40 + static_cast<int32_t>(std::round(double(level_) * level_ * .2375))
                          : demonHeal    ? DemonHealingData::Base(level_)
                                         : DwarfHealingData::Amount(level_);
            if (demonHeal)
                heal.perCondition = DemonHealingData::PerCondition(level_);
            pendingHeals_.push_back(heal);
        }
        if (absorption) {
            absorptionAt_ = context.now + Absorption::ActivationMs(medium_ == 1);
            absorptionUtc_ = context.wallNow + Absorption::ActivationMs(medium_ == 1);
            absorptionWater_ = medium_ == 1;
        }
        if (inspiring)
            inspiring_ = std::move(nextInspiring);
        if (greatDwarf) {
            greatDwarfAt_ = context.now + RiteAllies::Activation();
            greatDwarfUtc_ = context.wallNow + RiteAllies::Activation();
            greatDwarfWater_ = medium_ == 1;
        }
        if (facetRelease) {
            dragonRelease_ = std::move(nextRelease);
            dragonReleaseWater_ = medium_ == 1;
        }
        if (leap) {
            leapPoint_ = leapDestination;
            leapWater_ = medium_ == 1;
        }
        if (backstep) {
            evasion_.Begin(context.now);
            backstepAt_ = context.now + Backstep::Activation;
            backstepUtc_ = context.wallNow + Backstep::Activation;
            backstepWater_ = medium_ == 1;
        }
        if (approach) {
            approachAt_ = context.now + Approach::Activation;
            approachUtc_ = context.wallNow + Approach::Activation;
            approachTarget_ = target;
            approachWater_ = medium_ == 1;
        }
        if (instant) {
            lastInstantIdentity_ = identity;
            hasInstantIdentity_ = true;
        } else {
            phase_ = std::move(nextPhase);
            healCast_ = (legendAction || selfStealth || engineerHeal || simpleHeal) && !leap;
            if (initiativeEnabled)
                initiativeAnnounced_ = true;
            castEndsAt_ = context.now + duration;
            maintainedCast_ = maintained ? skillId : 0;
            lastSkillIdentity_ = identity;
            hasSkillIdentity_ = true;
            if (skill->requiresWeapon && slot == 5) {
                basicLast_ = skillId;
                basicUntil_ = castEndsAt_ <= UINT64_MAX - 6000 ? castEndsAt_ + 6000 : UINT64_MAX;
            } else if (backstep && context.taunted) {
                basicLast_ = 0;
                basicUntil_ = 0;
            }
            melee_ = {};
            if (beta::MeleeCombatSkill(167409, skillId) && (banish || skill->requiresWeapon) && !skill->rechargeMs &&
                target && (banish || skill->maximumGroundRange > 0) && context.combatTarget) {
                WorldCombatTarget enemy;
                if (context.combatTarget(target, enemy) && enemy.actor.agent == target &&
                    !enemy.actor.identity.empty()) {
                    melee_.skill = skillId;
                    melee_.identity = identity;
                    melee_.target = target;
                    melee_.targetIdentity = enemy.actor.identity;
                    melee_.minimum = skill->minimumGroundRange;
                    melee_.maximum = skill->maximumGroundRange;
                    melee_.medium = medium_;
                    if (banish) {
                        melee_.minimum = 0;
                        melee_.maximum = float(combat::BanishEnchantment::Range(167409));
                        melee_.settlesAt = context.now + 620;
                        melee_.impactUtc = context.wallNow + 620;
                    }
                }
            }
            if (jade) {
                melee_ = {};
                melee_.skill = 28406;
                melee_.identity = identity;
                melee_.medium = medium_;
                const auto activation = combat::JadeWinds::Activation(167409, medium_ == 1);
                melee_.settlesAt = context.now + activation;
                melee_.impactUtc = context.wallNow + activation;
            }
            if (engagement) {
                WorldCombatTarget enemy;
                if (context.combatTarget && context.combatTarget(target, enemy)) {
                    melee_ = {};
                    melee_.skill = 26679;
                    melee_.identity = identity;
                    melee_.target = target;
                    melee_.targetIdentity = enemy.actor.identity;
                    melee_.medium = medium_;
                    melee_.maximum = 900;
                    const auto event = combat::ForcedEngagement::Activation(167409, medium_ == 1);
                    melee_.settlesAt = context.now + event;
                    melee_.impactUtc = context.wallNow + event;
                }
            }
            pendingRecharge_ = skill->rechargeMs != 0;
            pendingRechargeIndex_ = skillIndex;
            rechargeStartsAt_ = context.now + skill->activationMs;
        }
        rechargeEndsAt_[skillIndex] = skill->rechargeMs ? context.now + rechargeDuration : 0;
        if (leap && context.combatEnemies && context.persistCombatHits) {
            melee_ = {};
            melee_.skill = 27917;
            melee_.identity = identity;
            melee_.medium = medium_;
            melee_.settlesAt = context.now + DemonLeap::Activation;
            melee_.impactUtc = context.wallNow + DemonLeap::Activation;
        }
        if (facetElements) {
            melee_ = {};
            melee_.skill = 27162;
            melee_.identity = identity;
            melee_.medium = medium_;
            melee_.settlesAt = context.now + combat::ElementalBlast::Activation(167409, medium_ == 1);
            melee_.impactUtc = context.wallNow + combat::ElementalBlast::Activation(167409, medium_ == 1);
        }
        if (facetBurst) {
            melee_ = {};
            melee_.skill = 28113;
            melee_.identity = identity;
            melee_.medium = medium_;
            melee_.settlesAt = context.now + combat::BurstStrengthImpact::Activation(167409, medium_ == 1);
            melee_.impactUtc = context.wallNow + combat::BurstStrengthImpact::Activation(167409, medium_ == 1);
        }
        if (phased || saveEffect || detonate)
            result.peerEffectVersion = ++skillVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool MeleeEquipped() const {
        for (const auto& skill : skills_)
            if (skill.skill == melee_.skill && skill.requiresWeapon && skill.equipmentSlot < definitions_.size() &&
                definitions_[skill.equipmentSlot] == skill.weapon)
                return true;
        return false;
    }
    bool MeleeDue(uint64_t now) const {
        if (!melee_.skill)
            return false;
        if (melee_.skill == 26679)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::ForcedEngagement::Legend(167409) || now >= melee_.settlesAt;
        if (melee_.skill == 27917)
            return !phase_.Active() || phase_.skill != 27917 || lastSkillIdentity_ != melee_.identity ||
                   medium_ != melee_.medium || legends_.Active() != combat::DemonLeapImpact::Legend(167409) ||
                   now >= melee_.settlesAt;
        if (melee_.skill == 27162)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::ElementalBlast::Legend(167409) || now >= melee_.settlesAt;
        if (melee_.skill == 28113)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::BurstStrengthImpact::Legend(167409) || now >= melee_.settlesAt;
        if (melee_.skill == 28406)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::JadeWinds::Legend(167409) || now >= melee_.settlesAt;
        if (melee_.skill == 27505)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::BanishEnchantment::Legend(167409) || now >= melee_.settlesAt;
        if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium || !MeleeEquipped())
            return true;
        if (phase_.Active())
            return phase_.skill != melee_.skill ||
                   (phase_.emitted && phase_.index + 1 == phase_.phases.size() && now >= phase_.endsAt);
        return !castEndsAt_ || now >= castEndsAt_;
    }
    WorldActionResult FinishMelee(const WorldActionContext& c, WorldActionOutput& out) {
        out = {};
        out.sequences = 0;
        if (melee_.skill == 26679) {
            if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                legends_.Active() != combat::ForcedEngagement::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27917) {
            if (!phase_.Active() || phase_.skill != 27917 || lastSkillIdentity_ != melee_.identity ||
                medium_ != melee_.medium || legends_.Active() != combat::DemonLeapImpact::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27162) {
            if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                legends_.Active() != combat::ElementalBlast::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 28113) {
            if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                legends_.Active() != combat::BurstStrengthImpact::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 28406) {
            if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                legends_.Active() != combat::JadeWinds::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (melee_.skill == 27505) {
            if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                legends_.Active() != combat::BanishEnchantment::Legend(167409)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(167409, peer_.character, level_, c, out);
            if (hit < 0) {
                combatRecovery_ = true;
                return WorldActionResult::Failed;
            }
            if (hit)
                melee_ = std::move(contact);
            return WorldActionResult::Handled;
        }
        if (lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium || !MeleeEquipped() ||
            (phase_.Active() ? phase_.skill != melee_.skill : !castEndsAt_)) {
            melee_ = {};
            return WorldActionResult::Handled;
        }
        WorldActionOutput end;
        end.sequences = 0;
        if (!phase_.Active()) {
            end.sequences = 1;
            if (!modern::SkillEnd(57344, c.sequence, lastSkillIdentity_, end.owned) ||
                !modern::SkillEnd(peer_.character, c.sequence, lastSkillIdentity_, end.peers))
                return WorldActionResult::Failed;
        }
        auto contact = melee_;
        const int hit = contact.Settle(167409, peer_.character, level_, c, out, &end);
        if (hit < 0) {
            combatRecovery_ = true;
            return WorldActionResult::Failed;
        }
        if (!hit)
            return WorldActionResult::Handled;
        melee_ = std::move(contact);
        phase_ = {};
        castEndsAt_ = 0;
        pendingRecharge_ = false;
        maintainedCast_ = 0;
        return WorldActionResult::Handled;
    }
    WorldActionResult Done(WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Notice(const char* text, WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        Guid sender;
        if (!China167409Game().ServiceChat(text, sender, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    InventoryState state_;
    PeerIdentity peer_;
    InventoryReports reports_, peerReports_;
    std::vector<InitialWeaponSkill> skills_;
    std::array<uint32_t, 68> definitions_{};
    std::array<bool, 68> bindings_{};
    uint64_t castEndsAt_ = 0;
    uint32_t maintainedCast_ = 0, lastSkillIdentity_ = 0;
    bool hasSkillIdentity_ = false;
    modern::PhasedSkill phase_;
    unsigned basicLast_ = 0;
    uint64_t basicUntil_ = 0;
    beta::MeleeContact melee_;
    bool combatRecovery_ = false;
    CatalogPosition leapPoint_;
    bool leapWater_ = false;
    modern::StarterMissiles missiles_;
    modern::RevenantBrand<167409> brand_;
    modern::Stealth<167409> stealth_;
    modern::Energy<167409> energy_;
    modern::LegendActions<167409> legends_;
    modern::Hammers<167409> hammers_;
    std::vector<modern::PendingHeal> pendingHeals_;
    bool healCast_ = false;
    uint64_t absorptionAt_ = 0, absorptionUtc_ = 0;
    bool absorptionWater_ = false, absorptionRecoveryRequired_ = false;
    uint8_t medium_ = 0;
    bool mediumReported_ = false;
    uint64_t skillVersion_ = 0;
    uint32_t lastInstantIdentity_ = 0;
    bool hasInstantIdentity_ = false;
    std::vector<uint64_t> rechargeEndsAt_;
    bool pendingRecharge_ = false;
    size_t pendingRechargeIndex_ = 0;
    uint64_t rechargeStartsAt_ = 0;
    std::vector<uint64_t> rechargeEndsUtc_;
    std::vector<bool> resumeReports_;
    bool rechargesRestored_ = false;
    uint8_t attunement_ = 0;
    unsigned level_ = 0;
    bool attunementReported_ = false;
    SelfEffect effect_;
    bool effectsNeedSave_ = false;
    modern::ConditionActions<167409> conditions_;
    Inspiring inspiring_;
    DragonRelease dragonRelease_;
    bool dragonReleaseWater_ = false;
    uint64_t greatDwarfAt_ = 0, greatDwarfUtc_ = 0;
    bool greatDwarfWater_ = false;
    uint64_t backstepAt_ = 0, backstepUtc_ = 0;
    bool backstepWater_ = false;
    uint64_t approachAt_ = 0, approachUtc_ = 0;
    uint32_t approachTarget_ = 0;
    bool approachWater_ = false;
    std::vector<uint8_t> approachMotion_;
};
} // namespace
bool InitializeInventory(const EquipmentCatalog& catalog, Character& character, std::string& error) {
    Character fresh = character;
    fresh.profileFields.erase("inventory_locations");
    fresh.profileFields.erase("inventory_bound");
    InitialEquipment equipment;
    InventoryState state;
    if (!catalog.ResolveInitial(fresh, equipment) || !LoadInventoryState(fresh, catalog, state, error)) {
        error = "Cannot resolve original China starting inventory";
        return false;
    }
    std::array<bool, 68> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (equipment.definitions[i]) {
            const auto* rule = FindStarterItemBinding(equipment.definitions[i]);
            if (!rule || (rule->onAcquisition != 0 && rule->onAcquisition != 2) ||
                (rule->onUse != 0 && rule->onUse != 2)) {
                error = "Missing original China starting binding rule";
                return false;
            }
            bindings[i] = rule->onAcquisition == 2 || rule->onUse == 2;
        }
    return state.StoreInitial(character, bindings, error);
}
bool LoadInventoryState(const Character& saved, const EquipmentCatalog& catalog, InventoryState& out,
                        std::string& error) {
    InitialEquipment original;
    if (!catalog.ResolveInitial(saved, original)) {
        error = "Cannot resolve original China inventory";
        return false;
    }
    return modern::LoadAquaticInventory(saved, original.definitions, catalog.InitialBag().capacity,
                                        catalog.HasAddedAquaticWeapon(saved), out, error);
}
bool CreateInventoryActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WorldActions>& out, std::string& error) {
    const auto initiativeField = saved.profileFields.find("initiative_state");
    if (initiativeField != saved.profileFields.end() && !initiativeField->second.empty()) {
        modern::InitiativeLedger value;
        const auto profession = saved.profileFields.find("profession");
        if (profession == saved.profileFields.end() || profession->second != "5" ||
            !value.Load(initiativeField->second)) {
            error = "Invalid saved initiative";
            return false;
        }
    }
    const auto enduranceField = saved.profileFields.find("endurance_state");
    if (enduranceField != saved.profileFields.end() && !enduranceField->second.empty()) {
        modern::EnduranceLedger value;
        const auto profession = saved.profileFields.find("profession");
        if (profession == saved.profileFields.end() || profession->second.size() != 1 || profession->second[0] < '1' ||
            profession->second[0] > '9' || !value.Load(enduranceField->second)) {
            error = "Invalid saved China endurance";
            return false;
        }
    }
    PeerIdentity expected;
    if (peer.player < 2 || peer.player == 255 || !China167409Bootstrap().PeerSlot(peer.player - 2, expected) ||
        peer.character != expected.character || peer.firstBag != expected.firstBag ||
        peer.itemBase != expected.itemBase) {
        error = "Invalid China inventory actor reservation";
        return false;
    }
    InventoryState state;
    if (!LoadInventoryState(saved, catalog, state, error))
        return false;
    modern::ConditionState<167409> conditionState;
    const auto conditionField = saved.profileFields.find("condition_effects");
    if (!conditionState.Load(conditionField == saved.profileFields.end() ? std::string() : conditionField->second)) {
        error = "Invalid saved China conditions";
        return false;
    }
    uint8_t attunement = 0;
    if (!SavedAttunement(saved, attunement)) {
        error = "Invalid saved China elemental attunement";
        return false;
    }
    const auto profession = saved.profileFields.find("profession");
    const unsigned effectProfession = profession != saved.profileFields.end() && profession->second == "9"   ? 9
                                      : profession != saved.profileFields.end() && profession->second == "5" ? 5
                                      : profession != saved.profileFields.end() && profession->second == "3" ? 3
                                      : attunement                                                           ? 6
                                                                                                             : 0;
    modern::Hammers<167409> hammers;
    Character legacyEffects;
    if (!hammers.Load(saved, legacyEffects)) {
        error = "Invalid saved China maintained skill";
        return false;
    }
    SelfEffect effect;
    if (!effect.Load(legacyEffects, effectProfession)) {
        error = "Invalid saved China skill effects";
        return false;
    }
    InitialEquipment initial;
    if (!catalog.ResolveInitial(saved, initial)) {
        error = "Cannot resolve original China weapon";
        return false;
    }
    const std::vector<InitialWeaponSkill>* skills = nullptr;
    if (!catalog.ResolveInitialWeaponSkills(saved, skills)) {
        error = "Cannot resolve original China skill actions";
        return false;
    }
    const std::vector<InitialWeaponSkill>* aquatic = nullptr;
    if (!catalog.ResolveAquaticWeaponSkills(saved, aquatic))
        return false;
    std::vector<InitialWeaponSkill> combined = *skills;
    combined.insert(combined.end(), aquatic->begin(), aquatic->end());
    float health = 0;
    if (!modern::ReadHealthState(saved, health)) {
        error = "Invalid saved China health state";
        return false;
    }
    if (profession != saved.profileFields.end() && profession->second == "6") {
        // Own palette116 in both environments, event0@1350 and one2067ms stage.
        // Only default mode4/flags0 healing; trait5579 and alternate models are excluded.
        InitialWeaponSkill heal;
        heal.skill = 5503;
        heal.nativeSlot = 0;
        heal.requiresWeapon = false;
        heal.minimumLevel = 1;
        heal.activationMs = 1350;
        heal.animationMs = 2067;
        heal.recoveryMs = 717;
        heal.rechargeMs = 20000;
        combined.push_back(heal);
    }
    if (profession != saved.profileFields.end() && profession->second == "3") {
        // Own aquatic palette276, event0@880 and stationary1760ms animation.
        InitialWeaponSkill heal;
        heal.skill = 5834;
        heal.nativeSlot = 0;
        heal.requiresWeapon = false;
        heal.minimumLevel = 1;
        heal.activationMs = 880;
        heal.animationMs = 1760;
        heal.recoveryMs = 880;
        heal.rechargeMs = 20000;
        combined.push_back(heal);
    }
    if (profession != saved.profileFields.end() && profession->second == "5") {
        // Own support palette268 and animation token: trigger1350, one2067ms stage.
        // Own refsTrait/predicate are null: heal138+round(level^2*.92),718/6000 at trigger.
        InitialWeaponSkill source;
        source.skill = modern::StealthRules::SelfSource;
        source.nativeSlot = 0;
        source.requiresWeapon = false;
        source.minimumLevel = 1;
        source.activationMs = 1350;
        source.animationMs = 2067;
        source.recoveryMs = 717;
        source.rechargeMs = 30000;
        combined.push_back(source);
        // Own aquatic palette133 and animation: marker1450 - recovery500 = trigger950.
        // Keep the complete 1667ms stage; use only the selected active heal.
        using Signet = modern::SignetMaliceHealing<167409>;
        InitialWeaponSkill signet;
        signet.skill = Signet::Skill;
        signet.nativeSlot = 0;
        signet.requiresWeapon = false;
        signet.minimumLevel = Signet::MinimumLevel;
        signet.activationMs = Signet::Activation;
        signet.animationMs = Signet::Animation;
        signet.recoveryMs = Signet::Animation - Signet::Activation;
        signet.rechargeMs = Signet::Recharge;
        combined.push_back(signet);
    }
    if (profession != saved.profileFields.end() && profession->second == "9") {
        InitialWeaponSkill engagement;
        engagement.skill = 26679;
        engagement.nativeSlot = 2;
        engagement.requiresWeapon = false;
        engagement.minimumLevel = 1;
        engagement.activationMs = 600;
        engagement.animationMs = 1233;
        engagement.recoveryMs = 633;
        engagement.rechargeMs = 15000;
        combined.push_back(engagement);
        InitialWeaponSkill jade;
        jade.skill = 28406;
        jade.nativeSlot = 4;
        jade.requiresWeapon = false;
        jade.minimumLevel = 1;
        jade.activationMs = 1200;
        jade.animationMs = 2033;
        jade.recoveryMs = 833;
        jade.rechargeMs = 5000;
        combined.push_back(jade);
        // 27505 stationary action from the already exported China records.
        InitialWeaponSkill banish;
        banish.skill = 27505;
        banish.nativeSlot = 2;
        banish.requiresWeapon = false;
        banish.minimumLevel = 1;
        banish.activationMs = 620;
        banish.animationMs = 1350;
        banish.recoveryMs = 730;
        banish.rechargeMs = 0;
        combined.push_back(banish);
        InitialWeaponSkill assassinHeal;
        assassinHeal.skill = 26937;
        assassinHeal.nativeSlot = 0;
        assassinHeal.requiresWeapon = false;
        assassinHeal.minimumLevel = 1;
        ConfigureAssassinInitialHeal(assassinHeal, false);
        combined.push_back(assassinHeal);
        InitialWeaponSkill heal;
        heal.skill = DwarfHealingData::Skill;
        heal.nativeSlot = DwarfHealingData::Slot;
        heal.requiresWeapon = false;
        heal.minimumLevel = DwarfHealingData::MinimumLevel;
        heal.activationMs = DwarfHealingData::ActivationMs;
        heal.animationMs = DwarfHealingData::AnimationMs;
        heal.rechargeMs = DwarfHealingData::RechargeMs;
        combined.push_back(heal);
        heal.skill = DemonHealingData::Skill;
        heal.activationMs = DemonHealingData::ActivationMs;
        heal.animationMs = DemonHealingData::AnimationMs;
        heal.recoveryMs = DemonHealingData::RecoveryMs;
        heal.rechargeMs = DemonHealingData::RechargeMs;
        combined.push_back(heal);
        InitialWeaponSkill absorption;
        absorption.skill = Absorption::Skill;
        absorption.nativeSlot = Absorption::Slot;
        absorption.requiresWeapon = false;
        absorption.minimumLevel = 1;
        absorption.activationMs = Absorption::ActivationMs(false);
        absorption.animationMs = Absorption::AnimationMs(false);
        absorption.recoveryMs = absorption.animationMs - absorption.activationMs;
        combined.push_back(absorption);
        InitialWeaponSkill rite;
        rite.skill = modern::GreatDwarfData<167409>::Skill;
        rite.nativeSlot = modern::GreatDwarfData<167409>::Slot;
        rite.requiresWeapon = false;
        rite.minimumLevel = modern::GreatDwarfData<167409>::MinimumLevel;
        rite.activationMs = modern::GreatDwarfData<167409>::ActivationMs;
        rite.animationMs = modern::GreatDwarfData<167409>::AnimationMs;
        combined.push_back(rite);
        InitialWeaponSkill hammer;
        hammer.skill = modern::Hammers<167409>::Skill;
        hammer.nativeSlot = modern::Hammers<167409>::Slot;
        hammer.requiresWeapon = false;
        hammer.minimumLevel = 1;
        hammer.activationMs = modern::Hammers<167409>::ActivationMs;
        hammer.animationMs = modern::Hammers<167409>::AnimationMs;
        combined.push_back(hammer);
        hammer.skill = modern::Hammers<167409>::StopSkill;
        hammer.activationMs = hammer.animationMs = 0;
        combined.push_back(hammer);
        for (uint32_t id : {28287u, 26693u}) {
            InitialWeaponSkill elite;
            elite.skill = id;
            elite.nativeSlot = 4;
            elite.requiresWeapon = false;
            elite.minimumLevel = 1;
            elite.activationMs = Maintained::ActivationFor(id, false);
            elite.animationMs = Maintained::AnimationFor(id, false);
            elite.recoveryMs = elite.animationMs - elite.activationMs;
            elite.rechargeMs = Maintained::RechargeFor(id);
            combined.push_back(elite);
        }
        for (uint32_t id : {27107u, 28382u}) {
            InitialWeaponSkill assassin;
            assassin.skill = id;
            assassin.nativeSlot = 3;
            assassin.requiresWeapon = false;
            assassin.minimumLevel = 1;
            combined.push_back(assassin);
        }
        for (unsigned id : {27220u, 27228u, 28379u, 27080u, 26644u, 28113u, 27014u, 27162u, 27760u, 28075u}) {
            const auto rule = Facets::RuleFor(id, false);
            InitialWeaponSkill facet;
            facet.skill = id;
            facet.nativeSlot = static_cast<uint8_t>(rule.slot);
            facet.requiresWeapon = false;
            facet.minimumLevel = 1;
            facet.activationMs = rule.activation;
            facet.animationMs = rule.animation;
            facet.recoveryMs = rule.animation - rule.activation;
            facet.rechargeMs = rule.recharge;
            combined.push_back(facet);
        }
        InitialWeaponSkill leap;
        leap.skill = DemonLeap::Skill;
        leap.nativeSlot = 3;
        leap.requiresWeapon = false;
        leap.minimumLevel = 1;
        ConfigureDemonLeap(leap, false);
        combined.push_back(leap);
        InitialWeaponSkill backstep;
        backstep.skill = Backstep::Skill;
        backstep.nativeSlot = 1;
        backstep.requiresWeapon = false;
        backstep.minimumLevel = 1;
        ConfigureBackstep(backstep, false);
        combined.push_back(backstep);
        InitialWeaponSkill approach;
        approach.skill = Approach::Skill;
        approach.nativeSlot = 2;
        approach.requiresWeapon = false;
        approach.minimumLevel = 1;
        ConfigureApproach(approach, false);
        combined.push_back(approach);
        InitialWeaponSkill field;
        field.skill = Inspiring::Skill;
        field.nativeSlot = 1;
        field.requiresWeapon = false;
        field.minimumLevel = 1;
        field.activationMs = 400;
        field.animationMs = 1233;
        field.recoveryMs = 833;
        field.rechargeMs = 10000;
        field.groundTargeted = true;
        field.maximumGroundRange = 900;
        combined.push_back(field);
    }
    std::vector<uint64_t> recharges;
    if (!LoadSkillRecharges(saved, combined, recharges, error))
        return false;
    InventoryReports reports, peerReports;
    std::array<bool, 68> bindings{};
    for (unsigned slot = 0; slot < state.locations.size(); ++slot)
        if (state.locations[slot] != -2) {
            const auto* rule = FindStarterItemBinding(initial.definitions[slot]);
            if (!rule) {
                error = "Missing original China starter binding rule";
                return false;
            }
            bindings[slot] = rule->onUse == 2;
            const uint32_t remote = static_cast<uint32_t>(peer.player) * 128 + slot + 1;
            if (!OriginalInventoryItemReport(catalog, saved, slot, 129 + slot, reports.unbound[slot]) ||
                !OriginalInventoryItemReport(catalog, saved, slot, 129 + slot, reports.bound[slot], true) ||
                !OriginalInventoryItemReport(catalog, saved, slot, remote, peerReports.unbound[slot], false, false) ||
                !OriginalInventoryItemReport(catalog, saved, slot, remote, peerReports.bound[slot], true, false)) {
                error = "Cannot prepare original China inventory item report";
                return false;
            }
        }
    modern::LegendActions<167409> legends;
    if (!legends.Load(saved)) {
        error = "Invalid saved China legends";
        return false;
    }
    out.reset(new InventoryActions(state, peer, std::move(reports), std::move(peerReports), std::move(combined),
                                   initial.definitions, bindings, std::move(recharges), attunement, saved.level,
                                   std::move(effect), std::move(legends), std::move(hammers)));
    error.clear();
    return true;
}
} // namespace china167409
} // namespace glide
