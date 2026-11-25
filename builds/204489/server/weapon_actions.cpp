#include "glide/profile/demon_leap_impact.h"
#include "glide/profile/elemental_blast.h"
#include "glide/profile/gaze_darkness.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/boon_removal.h"
#include "glide/profile/modern_brand.h"
#include "glide/profile/dodge_evasion.h"
#include "glide/profile/beta_melee_combat.h"
#include "glide/profile/modern_stealth.h"
#include "glide/profile/modern_energy.h"
#include "glide/profile/modern_hammers.h"
#include "glide/profile/modern_legends.h"
#include "glide/profile/modern_dwarf_effect.h"
#include "glide/profile/modern_conditions.h"
#include "glide/profile/pain_absorption.h"
#include "glide/profile/inspiring_field.h"
#include "glide/profile/dragon_release.h"
#include "glide/profile/burst_strength_data.h"
#include "glide/profile/great_dwarf_allies.h"
#include "glide/profile/target_approach.h"
#include "glide/profile/backstep_data.h"
#include "glide/profile/endurance.h"
#include "glide/profile/initiative.h"
#include "glide/profile/demon_leap_data.h"
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/regeneration.h"
#include "glide/profile/modern_revenant_sphere.h"
#include "aquatic_skills.h"
// Skill requests, timing and saved recharges reuse the accepted China controller.
// Build 204489 S34/S630 descriptors and original ability/animation data supply the wire values.
#include "weapon_actions.h"
#include "glide/profile/modern_missile.h"
#include "profession_state.h"
#include "health_state.h"
#include "air_aura_data.h"
#include "stance_rules.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <utility>

namespace glide {
const GameMessages& Build204489Game();
const GameBootstrap& Build204489Bootstrap();
namespace build204489 {
namespace {
using DemonHealingData = modern::DemonHealingData<204489>;
using Maintained = modern::Hammers<204489>;
using Absorption = modern::PainAbsorption<204489>;
using Inspiring = modern::InspiringField<204489>;
using Approach = modern::TargetApproach<204489>;
using Backstep = modern::BackstepData<204489>;
using DemonLeap = modern::DemonLeapData<204489>;
using Facets = modern::FacetData<204489>;
using DragonRelease = modern::DragonRelease<204489>;
using RiteAllies = modern::GreatDwarfAllies<204489>;
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
bool SkillBegin(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot, unsigned duration,
                uint32_t target, Buffer& out, bool underwater = false) {
    // Original unanimated skills use order 122. Its 11-byte payload carries
    // target, skill and slot; channel 1 leaves a timed channel-0 order intact.
    // AgBase requires a positive command lifetime, even without an animation.
    if (!duration)
        return out.U16(34) && out.U16(agent) && out.VarU32(sequence) && out.U8(0x30) && out.VarU32(identity) &&
               out.U8(11) && out.U8(122) && out.U8(0) && out.U32(target) && out.U32(skill) && out.U8(slot) &&
               out.U16(1);
    // Original S34 starts a timed stationary command. Its 13-byte behavior
    // creates ChCliOrder's native skill action (120), animation and slot state.
    // Byte12 bit0 makes Complete flush this step's markers and publish the
    // step completion. The separate S84 signal closes the whole skill.
    return duration && duration <= 65535 && out.U16(34) && out.U16(agent) && out.VarU32(sequence) && out.U8(0x10) &&
           out.VarU32(identity) && out.U8(13) && out.U8(120) && out.U8(0) && out.U32(skill) && out.U8(slot) &&
           out.U8(0) && out.U32(target) && out.U8(underwater ? 3 : 1) && out.U16(static_cast<uint16_t>(duration));
}
bool SkillEnd(uint16_t agent, uint32_t sequence, uint32_t identity, Buffer& out) {
    // S84 dispatches signal69 to the matching channel-0 skill order.
    // It closes an inactive step immediately or marks an active one as final.
    return out.U16(84) && out.U16(agent) && out.VarU32(sequence) && out.U8(0x10) && out.VarU32(identity) && out.U8(1) &&
           out.U8(69);
}
bool SkillRecharge(uint32_t skill, unsigned remaining, unsigned total, Buffer& out) {
    // Build 204489 S630's type 0 is the skill recharge queried by ChCliSkillbar.
    // Both durations are relative milliseconds; charges and slot timers differ.
    if (remaining > total)
        return false;
    Buffer bytes;
    const auto append = [&](uint32_t id) {
        return bytes.U16(630) && bytes.VarU32(57344) && bytes.VarU32(id) && bytes.U8(0) && bytes.VarU32(remaining) &&
               bytes.VarU32(total) && bytes.U16(0);
    };
    if (!append(skill))
        return false;
    // Same projection as the other builds, using this build's own message.
    const auto base = modern::FacetData<204489>::BaseFor(skill);
    if (base && base != skill && !append(base))
        return false;
    return out.Append(bytes.View());
}
struct SelfBuff {
    uint32_t definition = 0, durationMs = 0;
    SelfBuff(uint32_t id, unsigned duration) : definition(id), durationMs(duration) {}
};
using SelfBuffs = std::vector<SelfBuff>;
using PendingHeal = modern::PendingHeal;

struct TimedBuff {
    uint32_t definition = 0, instance = 0;
    uint64_t startsAt = 0, endsAt = 0;
    uint64_t startsUtc = 0, endsUtc = 0;
    bool announced = false;
};
bool BuffCreate(uint16_t actor, const TimedBuff& buff, uint64_t now, unsigned scalar, Buffer& out) {
    // S735: actor, source actor, Buff SkillDef, instance, milliseconds,
    // modifier scalar and active. The scalar's semantic name is still open.
    const uint64_t remaining = buff.endsAt > now ? buff.endsAt - now : 0;
    return remaining && remaining <= UINT32_MAX && scalar <= UINT8_MAX && out.U16(735) && out.VarU32(actor) &&
           out.VarU32(actor) && out.VarU32(buff.definition) && out.VarU32(buff.instance) &&
           out.VarU32(static_cast<uint32_t>(remaining)) && out.U8(static_cast<uint8_t>(scalar)) && out.U8(1);
}
bool BuffRemove(uint16_t actor, uint32_t instance, Buffer& out) {
    // S739 removes this instance from the combatant's active buff map.
    // S741 only removes an auxiliary entry and cannot replace this message.
    return out.U16(739) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(instance);
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
    error = "Invalid saved Build 204489 skill recharges";
    const auto& text = at->second;
    if (text.size() > skills.size() * 32)
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
        if (index == skills.size() || !skills[index].timingKnown || !skills[index].rechargeMs || out[index])
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
bool LoadBuffs(const Character& saved, const std::vector<SelfBuffs>& definitions, bool absorptionEnabled,
               std::vector<TimedBuff>& out, uint32_t& nextInstance, modern::Regeneration& regeneration,
               std::string& error) {
    const auto at = saved.profileFields.find("skill_effects");
    out.clear();
    nextInstance = 1;
    if (at == saved.profileFields.end() || at->second.empty())
        return true;
    error = "Invalid saved Build 204489 skill effects";
    std::string text;
    if (!regeneration.Load(at->second, text))
        return false;
    const size_t separator = text.find('|');
    uint64_t next = 0;
    if (text.size() > 8192 || text.compare(0, 2, "1:") || separator == std::string::npos ||
        !Decimal64(text.substr(2, separator - 2), next) || !next || next > UINT32_MAX)
        return false;
    nextInstance = static_cast<uint32_t>(next);
    std::set<uint32_t> identities;
    size_t begin = separator + 1;
    while (begin < text.size()) {
        const size_t end = text.find(',', begin), stop = end == std::string::npos ? text.size() : end;
        uint64_t values[4]{};
        size_t cursor = begin;
        for (size_t field = 0; field < 4; ++field) {
            const size_t colon = text.find(':', cursor), part = field == 3 ? stop : colon;
            if (part == std::string::npos || part > stop ||
                !Decimal64(text.substr(cursor, part - cursor), values[field]))
                return false;
            cursor = part + 1;
        }
        if (values[0] == 0 || values[0] >= modern::kFirstConditionInstance || values[0] >= next || values[1] == 0 ||
            values[1] >= 0x400000 || !values[2] || values[3] <= values[2] || out.size() >= 64 ||
            !identities.insert(static_cast<uint32_t>(values[0])).second)
            return false;
        const SelfBuff* definition = nullptr;
        for (const auto& group : definitions)
            for (const auto& candidate : group)
                if (candidate.definition == values[1] && candidate.durationMs == values[3] - values[2])
                    definition = &candidate;
        const bool resistance = absorptionEnabled && values[1] == 26980 &&
                                values[3] - values[2] <= modern::ConditionState<204489>::MaximumDuration;
        const bool receivedApproach =
            absorptionEnabled && values[1] == Approach::Buff && values[3] - values[2] == Approach::Duration;
        const bool receivedProtection =
            modern::AllowedReceivedEffect<204489>(static_cast<unsigned>(values[1]), values[3] - values[2]);
        const auto profession = saved.profileFields.find("profession");
        const bool backstepBoon = profession != saved.profileFields.end() && profession->second == "9" &&
                                  values[1] == Backstep::Boon() && values[3] - values[2] == Backstep::BoonDuration();
        if (!resistance && !receivedProtection && !receivedApproach && !backstepBoon &&
            (!definition || values[3] - values[2] != definition->durationMs))
            return false;
        TimedBuff buff;
        buff.instance = static_cast<uint32_t>(values[0]);
        buff.definition = static_cast<uint32_t>(values[1]);
        buff.startsUtc = values[2];
        buff.endsUtc = values[3];
        out.push_back(buff);
        if (end == std::string::npos)
            break;
        begin = end + 1;
        if (begin == text.size())
            return false;
    }
    error.clear();
    return true;
}
std::string LegacyBuffText(const std::vector<TimedBuff>& buffs, uint32_t nextInstance) {
    std::string text = "1:" + std::to_string(nextInstance) + "|";
    bool first = true;
    for (const auto& buff : buffs) {
        if (!first)
            text += ',';
        first = false;
        text += std::to_string(buff.instance) + ':' + std::to_string(buff.definition) + ':' +
                std::to_string(buff.startsUtc) + ':' + std::to_string(buff.endsUtc);
    }
    return text;
}
bool ReceiveProtection(std::vector<TimedBuff>& buffs, uint32_t& next, const ReceivedWorldEffect& request,
                       const WorldActionContext& c, unsigned level, uint16_t peer, WorldActionOutput& out,
                       unsigned& instance) {
    if (!c.wallNow || request.endsUtc <= c.wallNow ||
        !modern::AllowedReceivedEffect<204489>(request.definition, request.endsUtc - c.wallNow) ||
        c.now > UINT64_MAX - (request.endsUtc - c.wallNow))
        return false;
    auto found = buffs.end();
    if (request.refreshInstance) {
        found = std::find_if(buffs.begin(), buffs.end(),
                             [&](const TimedBuff& buff) { return buff.instance == request.refreshInstance; });
        if (found == buffs.end() || found->definition != request.definition || found->startsUtc > c.wallNow ||
            found->endsUtc <= c.wallNow)
            return false;
    } else if (buffs.size() >= 64 || !next || next >= modern::kFirstConditionInstance)
        return false;
    TimedBuff grant;
    grant.definition = request.definition;
    grant.instance = request.refreshInstance ? request.refreshInstance : next;
    grant.startsUtc = c.wallNow;
    grant.endsUtc = request.endsUtc;
    grant.startsAt = c.now;
    grant.endsAt = c.now + (request.endsUtc - c.wallNow);
    grant.announced = true;
    if (found != buffs.end() && found->announced &&
        (!BuffRemove(57344, found->instance, out.owned) || !BuffRemove(peer, found->instance, out.peers)))
        return false;
    if (!BuffCreate(57344, grant, c.now, level, out.owned) || !BuffCreate(peer, grant, c.now, level, out.peers))
        return false;
    if (found != buffs.end())
        *found = grant;
    else {
        buffs.push_back(grant);
        ++next;
    }
    instance = grant.instance;
    return true;
}
class WeaponActions final : public WeaponController {
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
        if (!combat::DemonLeapImpact::Conditions<204489>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t nominal,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ElementalBlast::Conditions<204489>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t nominal,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::BurstStrengthImpact::Conditions<204489>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 3, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t nominal,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::GazeDarkness::Conditions<204489>(c, nominal, received.conditionState))
            return WorldActionResult::Failed;
        const auto result = PrepareHostileRemoval(received, nominal, 2, out);
        if (result == WorldActionResult::Handled && out)
            out->beforeConditions = c.conditionState;
        return result;
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t nominal,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        auto received = c;
        if (!combat::ForcedEngagement::Conditions<204489>(c, nominal, received.conditionState))
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

        if (c.peerAgent != peer_.character || !c.wallNow || buffVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        struct Prepared final : PreparedHostileEffects {
            bool changed = false;
            WeaponActions& owner;
            std::vector<TimedBuff> buffs;
            modern::Regeneration regeneration;
            modern::Hammers<204489> hammers;
            modern::ConditionActions<204489> conditions;
            modern::Stealth<204489> stealth;
            Prepared(WeaponActions& value)
                : owner(value), buffs(value.buffs_), regeneration(value.regeneration_), hammers(value.hammers_),
                  conditions(value.conditions_), stealth(value.stealth_) {}
            void Commit() noexcept override {
                owner.buffs_ = std::move(buffs);
                owner.regeneration_ = std::move(regeneration);
                owner.effectsNeedSave_ = false;
                owner.hammers_ = std::move(hammers);
                owner.conditions_ = std::move(conditions);
                owner.stealth_ = std::move(stealth);
                if (changed)
                    ++owner.buffVersion_;
            }
        };
        auto next = std::unique_ptr<Prepared>(new Prepared(*this));
        std::vector<combat::LiveBoon> live, removed;
        for (const auto& e : next->buffs) {
            combat::LiveBoon r;
            r.instance = e.instance;
            r.definition = e.definition;
            r.starts = e.startsUtc;
            r.ends = e.endsUtc;
            r.announced = e.announced;
            live.push_back(r);
        }
        next->hammers.Boons(c, live);
        combat::BoonRemovals history;
        if (!history.Load(c.removedBoons) ||
            (operation < 2 && !(stability ? history.SelectContribution(live, 1122, c.wallNow, removed, nominal)
                                          : history.Select(live, combat::BanishEnchantment::RemovalLimit(204489),
                                                           c.wallNow, removed, nominal))))
            return WorldActionResult::Failed;
        const auto ids = combat::BoonRemovals::Instances(removed);
        next->buffs.erase(std::remove_if(next->buffs.begin(), next->buffs.end(),
                                         [&](const TimedBuff& e) { return ids.count(e.instance) != 0; }),
                          next->buffs.end());
        next->regeneration.Forget(ids);
        const auto removedText = history.Text();
        if (!next->hammers.SetRemovedBoons(removedText))
            return WorldActionResult::Failed;
        next->controlBlocked = stability && !removed.empty();
        std::string conditions = c.conditionState;
        if (operation == 0 && !combat::BanishEnchantment::Conditions<204489>(c, nominal, conditions))
            return WorldActionResult::Failed;
        if (operation) {
            modern::ConditionState<204489> ordinary;
            if (!ordinary.Load(conditions))
                return WorldActionResult::Failed;
            ordinary.Expire(c.wallNow);
            conditions = ordinary.Text();
        }
        auto changed = c;
        changed.conditionState = conditions;
        modern::ConditionActions<204489>::Prepared prepared;
        WorldActionOutput output;
        output.sequences = 0;
        if (operation == 2 && nominal <= UINT64_MAX - combat::GazeDarkness::Duration &&
            nominal + combat::GazeDarkness::Duration > c.wallNow && !next->stealth.Suppress(c, output))
            return WorldActionResult::Failed;
        if (!next->conditions.Prepare(changed, 0, level_, prepared, output) ||
            !combat::RemovedBoonMessages<204489>(57344, c.peerAgent, removed, output.owned, output.peers))
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
        const auto effects = prefix + (next->regeneration.Text(LegacyBuffText(next->buffs, nextBuffInstance_)));
        if (effects.size() > 8192)
            return WorldActionResult::Failed;
        next->beforeEffects = c.skillEffects;
        next->afterEffects = effects;
        next->beforeConditions = c.conditionState;
        next->afterConditions = conditions;
        next->beforeRemoved = c.removedBoons;
        next->afterRemoved = removedText;
        next->changed = output.owned.Size() || output.peers.Size();
        output.peerEffectVersion = buffVersion_ + (next->changed ? 1 : 0);
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
            context.peerAgent != peer_.character || buffVersion_ == UINT64_MAX || !context.wallNow ||
            context.skillEffects.size() > 8192)
            return WorldActionResult::Failed;
        auto candidate = buffs_;
        auto candidateRegeneration = regeneration_;
        uint32_t counter = nextBuffInstance_;
        if (!effectsRestored_) {
            for (auto& buff : candidate)
                if (buff.endsUtc > context.wallNow) {
                    const auto delay = buff.startsUtc > context.wallNow ? buff.startsUtc - context.wallNow : 0;
                    const auto remaining = buff.endsUtc - context.wallNow;
                    unsigned activation = 0;
                    for (size_t i = 0; i < selfBuffs_.size(); ++i)
                        for (const auto& effect : selfBuffs_[i])
                            if (effect.definition == buff.definition && skills_[i].activationMs > activation)
                                activation = skills_[i].activationMs;
                    if (delay > activation || remaining > buff.endsUtc - buff.startsUtc + activation ||
                        context.now > UINT64_MAX - remaining)
                        return WorldActionResult::Failed;
                    buff.startsAt = context.now + delay;
                    buff.endsAt = context.now + remaining;
                }
            candidate.erase(std::remove_if(candidate.begin(), candidate.end(),
                                           [&](const TimedBuff& buff) { return buff.endsUtc <= context.wallNow; }),
                            candidate.end());
        }
        WorldActionOutput output;
        output.sequences = 0;
        std::vector<unsigned> instances;
        std::set<unsigned> refreshed;
        for (const auto& incoming : requests) {
            auto request = incoming;
            if (combat::BoonRemovals::Removed(context.removedBoons, request.refreshInstance, request.definition,
                                              context.wallNow))
                request.refreshInstance = 0;
            if (request.refreshInstance && !refreshed.insert(request.refreshInstance).second)
                return WorldActionResult::Failed;
            unsigned instance = 0;
            if (!ReceiveProtection(candidate, counter, request, context, level_, peer_.character, output, instance))
                return WorldActionResult::Failed;
            if (request.definition == 718 &&
                !candidateRegeneration.Grant(instance, request.sourceLevel, context.wallNow, request.endsUtc))
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
            std::vector<TimedBuff>& destination;
            uint32_t& allocator;
            uint64_t& version;
            bool& dirty;
            std::vector<TimedBuff> candidate;
            uint32_t counter;
            modern::Regeneration& clock;
            modern::Regeneration nextClock;
            Prepared(std::vector<TimedBuff>& value, uint32_t& next, uint64_t& serial, bool& pending,
                     std::vector<TimedBuff>&& prepared, uint32_t count, modern::Regeneration& currentClock,
                     modern::Regeneration&& preparedClock)
                : destination(value), allocator(next), version(serial), dirty(pending), candidate(std::move(prepared)),
                  counter(count), clock(currentClock), nextClock(std::move(preparedClock)) {}
            void Commit() noexcept override {
                destination = std::move(candidate);
                allocator = counter;
                clock = std::move(nextClock);
                dirty = false;
                ++version;
            }
        };
        std::unique_ptr<Prepared> prepared(new Prepared(buffs_, nextBuffInstance_, buffVersion_, effectsNeedSave_,
                                                        std::move(candidate), counter, regeneration_,
                                                        std::move(candidateRegeneration)));
        prepared->before = context.skillEffects;
        prepared->after = prefix + prepared->nextClock.Text(LegacyBuffText(prepared->candidate, counter));
        if (prepared->after.size() > 8192)
            return WorldActionResult::Failed;
        prepared->instance = instances.front();
        prepared->instances = std::move(instances);
        output.peerEffectVersion = buffVersion_ + 1;
        prepared->output = std::move(output);
        out = std::move(prepared);
        return WorldActionResult::Handled;
    }
    void SetEquippedWeapons(const std::array<uint32_t, 68>& definitions) override {
        definitions_ = definitions;
    }
    WeaponActions(PeerIdentity peer, std::vector<InitialWeaponSkill> skills, std::array<uint32_t, 68> definitions,
                  std::vector<uint64_t> savedRecharges, unsigned level, std::vector<SelfBuffs> selfBuffs,
                  std::vector<TimedBuff> savedBuffs, uint32_t nextBuffInstance, std::vector<int32_t> selfHeals,
                  modern::Regeneration regeneration, uint8_t attunement, modern::LegendActions<204489> legends,
                  modern::Hammers<204489> hammers)
        : peer_(peer), level_(level), skills_(std::move(skills)), definitions_(definitions),
          rechargeEndsAt_(skills_.size(), 0), rechargeEndsUtc_(std::move(savedRecharges)),
          resumeReports_(skills_.size(), false), selfBuffs_(std::move(selfBuffs)), buffs_(std::move(savedBuffs)),
          nextBuffInstance_(nextBuffInstance), selfHeals_(std::move(selfHeals)), regeneration_(std::move(regeneration)),
          attunement_(attunement), energy_(std::any_of(skills_.begin(), skills_.end(),
                                                       [](const InitialWeaponSkill& s) { return s.skill == 26666; })),
          legends_(std::move(legends)), hammers_(std::move(hammers)) {}
    bool EvadesIncoming(uint64_t now) const override {
        return evasion_.Active(now);
    }
    uint64_t PeerEffectVersion() const override {
        return buffVersion_;
    }
    bool PeerCleanup(uint32_t, Buffer& out) const override {
        return missiles_.Cleanup(204489, out, peer_.character) && brand_.Cleanup(peer_.character, out) &&
               stealth_.Cleanup(peer_.character, out) && hammers_.Cleanup(peer_.character, out);
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        if (context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        // Existing observers receive each change through the normal broadcast.
        // A new observer needs the remaining time, never another full duration.
        if (seen)
            return WorldActionResult::Ignored;
        Buffer next;
        if (mediumReported_ && !modern::WeaponSelection(298, peer_.character, medium_, next))
            return WorldActionResult::Failed;
        if (attunementReported_ && !AttunementSelection(peer_.character, attunement_, next))
            return WorldActionResult::Failed;
        if (!legends_.Peer(peer_.character, next))
            return WorldActionResult::Failed;
        if (!conditions_.Snapshot(peer_.character, context.wallNow, level_, next))
            return WorldActionResult::Failed;
        for (const auto& buff : buffs_)
            if (buff.announced && context.now < buff.endsAt &&
                !BuffCreate(peer_.character, buff, context.now, level_, next))
                return WorldActionResult::Failed;
        if (!hammers_.Snapshot(peer_.character, context.now, level_, energy_, next))
            return WorldActionResult::Failed;
        if (!(phase_.skill == DemonLeap::Skill && context.now >= phase_.endsAt) &&
            !phase_.Snapshot(phase_.skill == Approach::Skill ? ApproachPhaseContext(context) : context, next))
            return WorldActionResult::Failed;
        if (!missiles_.Snapshot(204489, peer_.character, context.now, next))
            return WorldActionResult::Failed;
        if (!brand_.Snapshot(peer_.character, context.now, next) ||
            !stealth_.Snapshot(peer_.character, context.now, next))
            return WorldActionResult::Failed;
        if (!next.Size())
            return WorldActionResult::Ignored;
        out = std::move(next);
        return WorldActionResult::Handled;
    }
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
        const unsigned desired = c.now < basicUntil_ ? combat::BasicChainNext(204489, basicLast_) : 0;
        for (unsigned pass = 0; pass < 2; ++pass) {
            if (!desired && pass == 0)
                continue;
            for (const auto& s : skills_)
                if (s.requiresWeapon && s.slotType == 3 && s.underwater == (medium_ == 1) && s.minimumLevel <= level_ &&
                    s.timingKnown && !s.groundTargeted && MissileAvailable(s.skill)) {
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
                    return combat::ForcedEngagement::BasicRequest(204489, peer_.character, target, s.skill, token, out)
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
        if (!reader.U16(id))
            return WorldActionResult::Failed;
        if (id == modern::Legends<204489>::SelectRequest) {
            if (context.peerAgent != peer_.character)
                return WorldActionResult::Failed;
            if (!RestoreRecharges(context) || !RestoreBuffs(context))
                return WorldActionResult::Failed;
            return legends_.Request(input, context, level_, energy_, buffVersion_, out, LegendPersistence(context));
        }
        uint32_t dodgeToken = 0;
        if (context.persistCombatEndurance && modern::ModernDodgeToken(input, dodgeToken))
            return DodgeRequest(input, dodgeToken, context, out);
        if (!brand_.ObserveMotion(input))
            return WorldActionResult::Failed;
        Facing approachFacing;
        Buffer approachProbe;
        if (Build204489Game().DecodeFacing(input, approachFacing) == FacingResult::Present &&
            Build204489Game().LocalTeleport(input, {}, 1, approachProbe))
            approachMotion_.assign(input.data, input.data + input.size);
        if (id == 18)
            return MotionMedium(reader, context, out);
        if (id == 23 && (!RestoreRecharges(context) || !RestoreBuffs(context)))
            return WorldActionResult::Failed;
        return id == 23 ? SkillRequest(reader, context, out) : WorldActionResult::Ignored;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (combatRecovery_ || context.peerAgent != peer_.character || !context.sequence)
            return WorldActionResult::Failed;
        if (MeleeDue(context.now))
            return FinishMelee(context, out);
        const int projectile = missiles_.AdvanceCombat(204489, buffVersion_, context, out);
        if (projectile < 0) {
            combatRecovery_ = true;
            return WorldActionResult::Failed;
        }
        if (projectile == 0)
            return WorldActionResult::Handled;
        if (projectile == 2) {
            if (out.peerEffectVersion)
                buffVersion_ = out.peerEffectVersion;
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
        if (!RestoreRecharges(context) || !RestoreBuffs(context))
            return WorldActionResult::Failed;
        auto nextRegeneration = regeneration_;
        float regenerationHealth = 0, regenerationDelta = 0;
        bool regenerationChanged = false;
        if (!modern::ReadHealthState(context.healthState, regenerationHealth) ||
            !nextRegeneration.Prepare(LegacyBuffText(buffs_, nextBuffInstance_), context.wallNow, regenerationHealth,
                                      modern::kPublishedMaximumHealth, regenerationDelta, regenerationChanged))
            return WorldActionResult::Failed;
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        bool hammersChanged = false;
        WorldActionOutput hammersChanges;
        if (!nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, hammersChanges, hammersChanged) ||
            (hammersChanged && buffVersion_ == UINT64_MAX))
            return WorldActionResult::Failed;
        if (greatDwarfAt_ && (regenerationHealth <= 0 || legends_.Active() != modern::GreatDwarfData<204489>::Legend ||
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
        bool buffsReady = false;
        for (const auto& buff : buffs_)
            if (context.now >= buff.endsAt || InvalidStance(buff) || (!buff.announced && context.now >= buff.startsAt))
                buffsReady = true;
        bool resumeReady = false;
        for (bool pending : resumeReports_)
            if (pending)
                resumeReady = true;
        bool healsReady = false;
        for (const auto& heal : pendingHeals_)
            if (context.now >= heal.startsAt)
                healsReady = true;
        const bool selfSourceHealDue =
            std::any_of(pendingHeals_.begin(), pendingHeals_.end(), [&](const modern::PendingHeal& heal) {
                return (heal.skill == 13027 || heal.skill == 5834) && context.now >= heal.startsAt;
            });
        const bool retryableHealDue =
            std::any_of(pendingHeals_.begin(), pendingHeals_.end(), [&](const modern::PendingHeal& heal) {
                return (heal.skill == 26937 || heal.skill == 13027 || heal.skill == 5834 || heal.skill == 13050) &&
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
            backstepAt_ && (backstepHealth == 0 || legends_.Active() != 16 || backstepWater_ != (medium_ == 1));
        float leapHealth = 0;
        const bool leapActive = phase_.Active() && phase_.skill == DemonLeap::Skill;
        if (leapActive && !modern::ReadHealthState(context.healthState, leapHealth))
            return WorldActionResult::Failed;
        const bool cancelLeap =
            leapActive && (leapHealth == 0 || legends_.Active() != 18 || leapWater_ != (medium_ == 1) ||
                           (phase_.index == 0 && !DemonLeap::ValidPoint(context, leapPoint_)));
        const bool backstepDue = backstepAt_ && !cancelBackstep && context.now >= backstepAt_;
        if (backstepDue) {
            if (enduranceEnabled)
                resource.Gain(Backstep::EnduranceGain());
            modern::ConditionState<204489> candidate;
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
                modern::ConditionState<204489> candidate;
                if (!candidate.Load(conditionContext.conditionState))
                    return WorldActionResult::Failed;
                // Compatible local removal policy, not a recovered original operation:
                // own datatype4 primary refs737/723/736/861; preserve all other types.
                candidate.RemoveDefinitions({737, 723, 736, 861}, context.wallNow - elapsed);
                conditionContext.conditionState = candidate.Text();
                selfSourceConditions = true;
            }
        modern::ConditionActions<204489>::Prepared nextConditions;
        WorldActionOutput conditionChanges;
        if (!conditions_.Prepare(conditionContext, modern::DwarfConditionRemovalDue(pendingHeals_, context), level_,
                                 nextConditions, conditionChanges) ||
            ((nextConditions.changed || absorptionDue) && buffVersion_ == UINT64_MAX))
            return WorldActionResult::Failed;
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
        const bool enduranceUpdate = enduranceEnabled && !absorptionDue &&
                                     (!enduranceAnnounced_ || backstepDue || resource.Points() != endurancePoints_);
        if (enduranceEnabled && backstepDue && absorptionDue)
            return WorldActionResult::Failed;
        if (initiativeUpdate && absorptionDue)
            initiativeUpdate = false;
        if (!regenerationChanged && !castDone && !phaseReady && !rechargeReady && !resumeReady && !buffsReady &&
            !healsReady && !effectsNeedSave_ && !missileReady && !brandReady && !stealthReady && !energyReady &&
            !legends_.Due() && !nextConditions.changed && !nextConditions.save && !hammersChanged &&
            !hammers_.NeedsSave() && !absorptionDue && !cancelAbsorption && !backstepDue && !cancelBackstep &&
            !cancelLeap && !enduranceUpdate && !initiativeUpdate)
            return WorldActionResult::Ignored;
        if ((missileReady || brandReady || stealthReady) && buffVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        if (context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = castDone && !leapActive ? 1 : 0;
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
        const size_t phasePeerBytes = result.peers.Size();
        if (castDone && !leapActive &&
            (!context.sequence || context.sequence == UINT32_MAX ||
             !SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
             !SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers)))
            return WorldActionResult::Failed;
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
        std::vector<TimedBuff> nextBuffs = buffs_;
        uint32_t nextInstance = nextBuffInstance_;
        if (buffsReady) {
            nextBuffs.clear();
            if (!PrepareBuffs(context, result, nextBuffs, greatDwarfAt_ ? greatDwarfUtc_ + RiteAllies::Duration : 0))
                return WorldActionResult::Failed;
        }
        if (absorptionDue)
            for (const auto& grant : absorptionPlan.grants)
                if (!GrantAbsorption(grant, context, nextBuffs, nextInstance, result))
                    return WorldActionResult::Failed;
        bool backstepBoon = false;
        if (backstepDue) {
            if (context.wallNow < backstepUtc_ || backstepUtc_ > UINT64_MAX - Backstep::BoonDuration())
                return WorldActionResult::Failed;
            const auto elapsed = context.wallNow - backstepUtc_;
            if (elapsed < Backstep::BoonDuration()) {
                if (elapsed > context.now || context.now - elapsed > UINT64_MAX - Backstep::BoonDuration() ||
                    nextBuffs.size() >= 64 || nextInstance >= modern::kFirstConditionInstance ||
                    buffVersion_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                TimedBuff buff;
                buff.instance = nextInstance++;
                buff.definition = Backstep::Boon();
                buff.startsAt = context.now - elapsed;
                buff.endsAt = buff.startsAt + Backstep::BoonDuration();
                buff.startsUtc = backstepUtc_;
                buff.endsUtc = backstepUtc_ + Backstep::BoonDuration();
                buff.announced = true;
                if (!BuffCreate(57344, buff, context.now, level_, result.owned) ||
                    !BuffCreate(peer_.character, buff, context.now, level_, result.peers))
                    return WorldActionResult::Failed;
                nextBuffs.push_back(buff);
                backstepBoon = true;
                buffsReady = true;
            }
        }
        std::vector<PendingHeal> nextHeals;
        float previousHealth = 0, currentHealth = 0;
        if (healsReady && !modern::PrepareSelfHeals<204489>(pendingHeals_, context, peer_.character, nextHeals,
                                                            previousHealth, currentHealth, result))
            return WorldActionResult::Failed;
        bool selfRegenerationAdded = false;
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
                if ((heal.skill != 13027 && heal.skill != 5834) || context.now < heal.startsAt)
                    continue;
                const uint64_t elapsed = context.now - heal.startsAt;
                for (unsigned definition : {717u, 718u, 719u}) {
                    if (heal.skill == 13027 && definition != 718)
                        continue;
                    const unsigned duration = heal.skill == 13027 ? 6000 : definition == 717 ? 2000 : 4000;
                    if (elapsed >= duration)
                        continue;
                    if (elapsed >= context.wallNow || context.now > UINT64_MAX - duration ||
                        context.wallNow > UINT64_MAX - duration || nextBuffs.size() >= 64 ||
                        nextInstance >= modern::kFirstConditionInstance || buffVersion_ == UINT64_MAX)
                        return WorldActionResult::Failed;
                    TimedBuff buff;
                    buff.instance = nextInstance++;
                    buff.definition = definition;
                    buff.startsAt = heal.startsAt;
                    buff.endsAt = buff.startsAt + duration;
                    buff.startsUtc = context.wallNow - elapsed;
                    buff.endsUtc = buff.startsUtc + duration;
                    buff.announced = true;
                    if ((definition == 718 &&
                         !nextRegeneration.Grant(buff.instance, level_, buff.startsUtc, buff.endsUtc)) ||
                        !BuffCreate(57344, buff, context.now, level_, result.owned) ||
                        !BuffCreate(peer_.character, buff, context.now, level_, result.peers))
                        return WorldActionResult::Failed;
                    nextBuffs.push_back(buff);
                    selfRegenerationAdded = true;
                    buffsReady = true;
                }
            }
        // Announcing changes no saved timestamp. Removing an effect and clearing
        // expired restored entries must finish storage before either wire view.
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
            if (delta > 0 && (!modern::HealthChange<204489>(57344, 718, delta, result.owned) ||
                              !modern::HealthChange<204489>(peer_.character, 718, delta, result.peers)))
                return WorldActionResult::Failed;
        }
        const bool saveHealth =
            regenerationChanged || selfSourceHealDue || (healsReady && currentHealth != previousHealth);
        const bool saveEffects = backstepBoon || effectsNeedSave_ ||
                                 (buffsReady && nextBuffs.size() != buffs_.size()) || hammersChanged ||
                                 nextHammers.NeedsSave() || (nextHammers.Tracked() && energyReady);
        // The selected target gets its remapped motion prefix plus the same
        // actor buffs/heals as other observers in this tick.
        if (result.targetPeerAgent && result.peers.Size() > phasePeerBytes &&
            !result.targetView.Append(
                ByteView(result.peers.Data() + phasePeerBytes, result.peers.Size() - phasePeerBytes)))
            return WorldActionResult::Failed;
        auto nextMissiles = missiles_;
        auto nextBrand = brand_;
        auto nextStealth = stealth_;
        if (!result.owned.Append(hammersChanges.owned.View()) || !result.peers.Append(hammersChanges.peers.View()))
            return WorldActionResult::Failed;
        auto nextLegends = legends_;
        if (!nextLegends.Initial(context, result))
            return WorldActionResult::Failed;
        if (energyReady && !nextEnergy.Advance(context.now, result))
            return WorldActionResult::Failed;
        if (missileReady && !nextMissiles.Advance(204489, context, available, result))
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
        if (!result.owned.Append(conditionChanges.owned.View()) ||
            !result.peers.Append(conditionChanges.peers.View()) ||
            (result.targetPeerAgent && !result.targetView.Append(conditionChanges.peers.View())))
            return WorldActionResult::Failed;
        if (initiativeUpdate && !modern::InitiativeCurrent(204489, 57344, initiative, result.owned))
            return WorldActionResult::Failed;
        if (enduranceUpdate && !modern::EnduranceCurrent(204489, 57344, resource.Points(), result.owned))
            return WorldActionResult::Failed;
        if ((assassinActivation || initiativeUpdate || enduranceUpdate || saveHealth || saveEffects ||
             nextConditions.save || absorptionDue) &&
            !context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (absorptionDue) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!modern::WriteHealthState(currentHealth, health) || !context.persistConditionTransfer)
                return WorldActionResult::Failed;
            const auto committed = context.persistConditionTransfer(
                RechargeText(rechargeEndsUtc_),
                nextHammers.Text(nextRegeneration.Text(LegacyBuffText(nextBuffs, nextInstance)), nextEnergy, context),
                health, nextConditions.state.Text(), absorptionPlan.donors);
            if (committed == WorldConditionCommit::RecoveryRequired) {
                absorptionRecoveryRequired_ = true;
                return WorldActionResult::Failed;
            }
            if (committed == WorldConditionCommit::NotApplied)
                return Done(out);
        } else if (assassinActivation) {
            std::string health;
            if (!modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            if (!context.persistAssassinActivation(
                    RechargeText(rechargeEndsUtc_),
                    nextHammers.Text(
                        nextRegeneration.Text(LegacyBuffText(buffsReady ? nextBuffs : buffs_, nextInstance)),
                        nextEnergy, context),
                    health, nextConditions.state.Text(), enduranceUpdate ? resource.Text() : context.enduranceState,
                    assassinActivation))
                return Done(out);
        } else if (initiativeUpdate) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            const bool stored =
                enduranceUpdate
                    ? (context.persistCombatResources &&
                       context.persistCombatResources(
                           RechargeText(rechargeEndsUtc_),
                           nextHammers.Text(
                               nextRegeneration.Text(LegacyBuffText(buffsReady ? nextBuffs : buffs_, nextInstance)),
                               nextEnergy, context),
                           health, nextConditions.state.Text(), resource.Text(), initiative.Text()))
                    : context.persistCombatInitiative(
                          RechargeText(rechargeEndsUtc_),
                          nextHammers.Text(
                              nextRegeneration.Text(LegacyBuffText(buffsReady ? nextBuffs : buffs_, nextInstance)),
                              nextEnergy, context),
                          health, nextConditions.state.Text(), initiative.Text());
            if (!stored)
                return Done(out);
        } else if (enduranceUpdate) {
            if (!healsReady && !regenerationChanged && !modern::ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!modern::WriteHealthState(currentHealth, health))
                return WorldActionResult::Failed;
            if (!context.persistCombatEndurance(RechargeText(rechargeEndsUtc_),
                                                nextHammers.Text(nextRegeneration.Text(LegacyBuffText(
                                                                     buffsReady ? nextBuffs : buffs_, nextInstance)),
                                                                 nextEnergy, context),
                                                health, nextConditions.state.Text(), resource.Text()))
                return Done(out);
        } else if (nextConditions.save) {
            if (!healsReady && !regenerationChanged && !ReadHealthState(context.healthState, currentHealth))
                return WorldActionResult::Failed;
            std::string health;
            if (!WriteHealthState(currentHealth, health) || !context.persistCombatConditions)
                return WorldActionResult::Failed;
            if (!context.persistCombatConditions(RechargeText(rechargeEndsUtc_),
                                                 nextHammers.Text(nextRegeneration.Text(LegacyBuffText(
                                                                      buffsReady ? nextBuffs : buffs_, nextInstance)),
                                                                  nextEnergy, context),
                                                 health, nextConditions.state.Text()))
                return (retryableHealDue || regenerationChanged) ? Done(out) : WorldActionResult::Failed;
        } else if (saveHealth) {
            std::string health;
            if (!WriteHealthState(currentHealth, health) || !context.persistCombatState)
                return WorldActionResult::Failed;
            if (!context.persistCombatState(RechargeText(rechargeEndsUtc_),
                                            nextHammers.Text(nextRegeneration.Text(LegacyBuffText(
                                                                 buffsReady ? nextBuffs : buffs_, nextInstance)),
                                                             nextEnergy, context),
                                            health))
                return (retryableHealDue || regenerationChanged) ? Done(out) : WorldActionResult::Failed;
        } else if (saveEffects &&
                   (!context.persistSkillState ||
                    !context.persistSkillState(RechargeText(rechargeEndsUtc_),
                                               nextHammers.Text(nextRegeneration.Text(LegacyBuffText(
                                                                    buffsReady ? nextBuffs : buffs_, nextInstance)),
                                                                nextEnergy, context))))
            return WorldActionResult::Failed;
        if (rechargeReady)
            pendingRecharge_ = false;
        if (castDone || (phaseReady && !nextPhase.Active()))
            castEndsAt_ = 0;
        phase_ = std::move(nextPhase);
        missiles_ = std::move(nextMissiles);
        brand_ = std::move(nextBrand);
        stealth_ = std::move(nextStealth);
        energy_ = std::move(nextEnergy);
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        legends_ = std::move(nextLegends);
        if (buffsReady || absorptionDue)
            buffs_ = std::move(nextBuffs);
        if (absorptionDue || selfRegenerationAdded || backstepBoon)
            nextBuffInstance_ = nextInstance;
        if (buffsReady || missileReady || brandChanged || stealthChanged || nextConditions.changed || hammersChanged ||
            absorptionDue)
            ++buffVersion_;
        if (healsReady)
            pendingHeals_ = std::move(nextHeals);
        if ((buffsReady || missileReady || brandChanged || stealthChanged || nextConditions.changed || hammersChanged ||
             absorptionDue) &&
            result.peers.Size())
            result.peerEffectVersion = buffVersion_;
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
        regeneration_ = std::move(nextRegeneration);
        effectsNeedSave_ = false;
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
        const auto& game = Build204489Game();
        CatalogPosition position;
        Facing facing;
        float health = 0;
        if (!combat::DodgeEvasion::CanBegin(context.now) || context.peerAgent != peer_.character || !context.sequence ||
            context.sequence > UINT32_MAX - 2 || token <= lastDodgeToken_ || buffVersion_ == UINT64_MAX ||
            !game.NeedsActionTickBeforeMotion(input) || (backstepAt_ && context.now >= backstepAt_))
            return Done(out);
        if (game.DecodePosition(input, position) == PositionResult::Invalid ||
            game.DecodeFacing(input, facing) == FacingResult::Invalid ||
            !modern::ReadHealthState(context.healthState, health) ||
            !(RestoreRecharges(context) && RestoreBuffs(context)))
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
            !modern::EnduranceCurrent(204489, 57344, resource.Points(), result.owned))
            return WorldActionResult::Failed;
        std::vector<TimedBuff> remainingBuffs;
        for (const auto& buff : buffs_)
            if (buff.announced || buff.startsAt <= context.now)
                remainingBuffs.push_back(buff);
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!context.persistCombatEndurance(
                RechargeText(rechargeEndsUtc_),
                hammers_.Text(regeneration_.Text(LegacyBuffText(remainingBuffs, nextBuffInstance_)), energy_, context),
                context.healthState, context.conditionState, resource.Text()))
            return Done(out);
        buffs_ = std::move(remainingBuffs);
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
        result.peerEffectVersion = ++buffVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    std::string BuffText(const std::vector<TimedBuff>& buffs, uint32_t next) const {
        return regeneration_.Text(LegacyBuffText(buffs, next));
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
        if (c.peerAgent != peer_.character || buffVersion_ == UINT64_MAX) {
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
        if (health <= 0 || legends_.Active() != 16 || approachWater_ != (medium_ == 1) ||
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
            cancelled.peerEffectVersion = ++buffVersion_;
            out = std::move(cancelled);
            return WorldActionResult::Handled;
        }
        if (c.now < approachAt_)
            return WorldActionResult::Ignored;
        handled = true;
        if (c.peerAgent != peer_.character || !c.wallNow || buffVersion_ == UINT64_MAX)
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
                !Build204489Game().LocalTeleport({approachMotion_.data(), approachMotion_.size()}, destination,
                                                 c.sequence + result.sequences, ownTeleport) ||
                !Build204489Game().PeerTeleport({approachMotion_.data(), approachMotion_.size()}, destination,
                                                peer_.character, c.sequence + result.sequences, peerTeleport) ||
                !result.owned.Append(ownTeleport.View()) || !result.peers.Append(peerTeleport.View()))
                return WorldActionResult::Failed;
            result.sequences += 2;
            result.relocate = true;
            result.destination = destination;
        }
        // Deadline is the accepted cast's trigger, never a fresh five seconds on retry.
        const uint64_t elapsed = c.now - approachAt_;
        std::vector<TimedBuff> own;
        if (!PrepareBuffs(c, result, own))
            return WorldActionResult::Failed;
        uint32_t counter = nextBuffInstance_;
        if (elapsed < Approach::Duration) {
            if (counter >= modern::kFirstConditionInstance)
                return WorldActionResult::Failed;
            for (const auto& buff : own)
                if (buff.definition == Approach::Buff && buff.announced &&
                    (!BuffRemove(57344, buff.instance, result.owned) ||
                     !BuffRemove(peer_.character, buff.instance, result.peers)))
                    return WorldActionResult::Failed;
            own.erase(std::remove_if(own.begin(), own.end(),
                                     [](const TimedBuff& b) { return b.definition == Approach::Buff; }),
                      own.end());
            if (own.size() >= 64)
                return WorldActionResult::Failed;
            TimedBuff buff;
            buff.definition = Approach::Buff;
            buff.instance = counter++;
            buff.startsAt = approachAt_;
            buff.endsAt = approachAt_ + Approach::Duration;
            buff.startsUtc = approachUtc_;
            buff.endsUtc = approachUtc_ + Approach::Duration;
            if (!BuffCreate(57344, buff, c.now, level_, result.owned) ||
                !BuffCreate(peer_.character, buff, c.now, level_, result.peers))
                return WorldActionResult::Failed;
            buff.announced = true;
            own.push_back(buff);
        }
        if (result.targetPeerAgent && result.peers.Size() > phasePeerBytes &&
            !result.targetView.Append({result.peers.Data() + phasePeerBytes, result.peers.Size() - phasePeerBytes}))
            return WorldActionResult::Failed;
        if (!c.ReserveSequences(result.sequences) || !c.persistSkillState)
            return WorldActionResult::Failed;
        if (!c.persistSkillState(RechargeText(rechargeEndsUtc_),
                                 nextHammers.Text(BuffText(own, counter), nextEnergy, c)))
            return Done(out);
        buffs_ = std::move(own);
        nextBuffInstance_ = counter;
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        phase_ = std::move(nextPhase);
        if (!phase_.Active())
            castEndsAt_ = 0;
        approachAt_ = approachUtc_ = 0;
        ++buffVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = buffVersion_;
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
        if (health <= 0 || legends_.Active() != modern::GreatDwarfData<204489>::Legend ||
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
        if (status != modern::GreatDwarfAlliesStatus::Ready || buffVersion_ == UINT64_MAX ||
            !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        std::vector<TimedBuff> own;
        auto counter = nextBuffInstance_;
        if (!PrepareBuffs(c, result, own))
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
            effects = nextHammers.Text(BuffText(own, counter), nextEnergy, c);
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
        buffs_ = std::move(own);
        nextBuffInstance_ = counter;
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++buffVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = buffVersion_;
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
        if (status != modern::DragonReleaseStatus::Ready || buffVersion_ == UINT64_MAX ||
            !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        std::vector<TimedBuff> own;
        auto counter = nextBuffInstance_;
        if (!PrepareBuffs(c, result, own))
            return WorldActionResult::Failed;
        for (const auto& grant : plan.own) {
            unsigned id = 0;
            if (!ReceiveProtection(own, counter, grant, c, level_, peer_.character, result, id))
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
            effects = nextHammers.Text(BuffText(own, counter), nextEnergy, c);
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
        buffs_ = std::move(own);
        nextBuffInstance_ = counter;
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++buffVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = buffVersion_;
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
        auto nextRegeneration = regeneration_;
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
        if (status != modern::FacetPulseStatus::Ready || buffVersion_ == UINT64_MAX || !c.persistPreparedAlliedEffects)
            return WorldActionResult::Failed;
        if (!nextEnergy.Advance(c.now, result))
            return WorldActionResult::Failed;
        std::vector<unsigned> ownInstances;
        std::vector<TimedBuff> own;
        auto counter = nextBuffInstance_;
        if (!PrepareBuffs(c, result, own))
            return WorldActionResult::Failed;
        for (auto grant : plan.own) {
            if (grant.definition == 718)
                grant.sourceLevel = level_;
            unsigned id = 0;
            if (!ReceiveProtection(own, counter, grant, c, level_, peer_.character, result, id))
                return WorldActionResult::Failed;
            if (grant.definition == 718 && !nextRegeneration.Grant(id, grant.sourceLevel, c.wallNow, grant.endsUtc))
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
            effects = nextHammers.Text(nextRegeneration.Text(LegacyBuffText(own, counter)), nextEnergy, c);
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
        regeneration_ = std::move(nextRegeneration);
        buffs_ = std::move(own);
        nextBuffInstance_ = counter;
        effectsNeedSave_ = false;
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        ++buffVersion_;
        if (result.peers.Size())
            result.peerEffectVersion = buffVersion_;
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
        if (health <= 0 || legends_.Active() != 17) {
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
        if (status != modern::InspiringFieldStatus::Ready || buffVersion_ == UINT64_MAX || !c.persistAlliedEffects)
            return WorldActionResult::Failed;
        auto own = buffs_;
        auto counter = nextBuffInstance_;
        WorldActionOutput result;
        result.sequences = 0;
        unsigned instance = 0;
        std::string effects = c.skillEffects;
        if (plan.own) {
            if (!ReceiveProtection(own, counter, plan.ownEffect, c, level_, peer_.character, result, instance))
                return WorldActionResult::Failed;
            std::string prefix;
            if (effects.compare(0, 3, "H1:") == 0 || effects.compare(0, 3, "H2:") == 0 ||
                effects.compare(0, 3, "H3:") == 0) {
                const auto bar = effects.find('|');
                if (bar == std::string::npos)
                    return WorldActionResult::Failed;
                prefix = effects.substr(0, bar + 1);
            }
            effects = prefix + BuffText(own, counter);
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
            buffs_ = std::move(own);
            nextBuffInstance_ = counter;
            effectsNeedSave_ = false;
            result.peerEffectVersion = ++buffVersion_;
        }
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool GrantAbsorption(const modern::PainAbsorptionGrant& grant, const WorldActionContext& c,
                         std::vector<TimedBuff>& buffs, uint32_t& nextInstance, WorldActionOutput& out) const {
        if ((grant.buff != 26980 && grant.buff != 873) || !grant.startsUtc || grant.startsUtc > c.wallNow ||
            grant.endsUtc <= c.wallNow || grant.endsUtc <= grant.startsUtc ||
            nextInstance >= modern::kFirstConditionInstance)
            return false;
        uint64_t remaining = grant.endsUtc - c.wallNow;
        bool merged = false;
        if (grant.buff == 26980)
            for (const auto& buff : buffs)
                if (buff.definition == 26980 && buff.endsAt > c.now) {
                    remaining += buff.endsAt - c.now;
                    merged = true;
                }
        // Shared technical safety ceiling; not an authored resistance cap.
        if (remaining > modern::ConditionState<204489>::MaximumDuration || c.now > UINT64_MAX - remaining ||
            c.wallNow > UINT64_MAX - remaining)
            return false;
        if (grant.buff == 873 && grant.endsUtc - grant.startsUtc != 5000)
            return false;
        if (grant.buff == 26980) {
            for (const auto& buff : buffs)
                if (buff.definition == 26980 && buff.announced &&
                    (!BuffRemove(57344, buff.instance, out.owned) ||
                     !BuffRemove(peer_.character, buff.instance, out.peers)))
                    return false;
            buffs.erase(std::remove_if(buffs.begin(), buffs.end(),
                                       [](const TimedBuff& buff) { return buff.definition == 26980; }),
                        buffs.end());
        }
        if (buffs.size() >= 64)
            return false;
        TimedBuff buff;
        buff.definition = grant.buff;
        buff.instance = nextInstance++;
        buff.startsUtc = merged ? c.wallNow : grant.startsUtc;
        buff.endsUtc = c.wallNow + remaining;
        const auto elapsed = c.wallNow - buff.startsUtc;
        buff.startsAt = c.now >= elapsed ? c.now - elapsed : 0;
        buff.endsAt = c.now + remaining;
        if (!BuffCreate(57344, buff, c.now, level_, out.owned) ||
            !BuffCreate(peer_.character, buff, c.now, level_, out.peers))
            return false;
        buff.announced = true;
        buffs.push_back(buff);
        return true;
    }
    modern::LegendActions<204489>::Persist LegendPersistence(const WorldActionContext& context) {
        return [this, &context](const std::string& legends, unsigned active, modern::Energy<204489>& energy,
                                WorldActionOutput& result) {
            if (!hammers_.Tracked()) {
                if (!context.persistLegendState || !context.persistLegendState(legends))
                    return false;
                if (active != 16)
                    CancelAssassinInitialHeal();
                if (active != Facets::Legend)
                    dragonRelease_.Cancel();
                if (active != modern::GreatDwarfData<204489>::Legend)
                    greatDwarfAt_ = greatDwarfUtc_ = 0;
                if (active != Absorption::Legend())
                    absorptionAt_ = absorptionUtc_ = 0;
                return true;
            }
            auto next = hammers_;
            auto nextBuffs = buffs_;
            if (active != Facets::Legend)
                nextBuffs.erase(std::remove_if(nextBuffs.begin(), nextBuffs.end(),
                                               [](const TimedBuff& buff) {
                                                   return buff.definition == modern::BurstStrength::Buff &&
                                                          !buff.announced;
                                               }),
                                nextBuffs.end());
            if (active != Maintained::LegendFor(next.CurrentSkill()) && !next.Stop(context, energy, result))
                return false;
            if (!context.persistLegendSkillState ||
                !context.persistLegendSkillState(legends, RechargeText(rechargeEndsUtc_),
                                                 next.Text(BuffText(nextBuffs, nextBuffInstance_), energy, context)))
                return false;
            next.Saved();
            hammers_ = std::move(next);
            buffs_ = std::move(nextBuffs);
            effectsNeedSave_ = false;
            if (active != 16)
                CancelAssassinInitialHeal();
            if (active != Facets::Legend)
                dragonRelease_.Cancel();
            if (active != modern::GreatDwarfData<204489>::Legend)
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
                       definitions_[skill.equipmentSlot] == skill.weapon;
        return false;
    }
    WorldActionResult SelectAttunement(uint32_t skill, uint8_t slot, uint32_t identity,
                                       const WorldActionContext& context, WorldActionOutput& out) {
        const auto selected = static_cast<uint8_t>(slot - 11);
        if (!attunement_ || selected < 1 || selected > 4 || skill != 5491u + selected ||
            level_ < AttunementLevel(selected))
            return Done(out);
        if (identity == lastInstantIdentity_ && hasInstantIdentity_)
            return Done(out);
        if (buffVersion_ == UINT64_MAX)
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
        std::vector<TimedBuff> remainingBuffs;
        for (const auto& buff : buffs_)
            if (!changed || buff.announced || buff.startsAt <= context.now)
                remainingBuffs.push_back(buff);
        if (!context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (changed && (!context.persistProfessionSkillState ||
                        !context.persistProfessionSkillState(
                            selected, RechargeText(rechargeEndsUtc_),
                            hammers_.Text(BuffText(remainingBuffs, nextBuffInstance_), energy_, context))))
            return Done(out);
        if (changed) {
            buffs_ = std::move(remainingBuffs);
            pendingHeals_.erase(std::remove_if(pendingHeals_.begin(), pendingHeals_.end(),
                                               [&](const PendingHeal& heal) {
                                                   return heal.startsAt > context.now || heal.skill == 13027 ||
                                                          heal.skill == 5834 || heal.skill == 13050;
                                               }),
                                pendingHeals_.end());
            phase_ = modern::PhasedSkill{};
            castEndsAt_ = 0;
            pendingRecharge_ = false;
            missiles_.CancelPending();
            brand_.CancelWarp();
            stealth_.CancelPending();
        }
        attunement_ = selected;
        attunementReported_ = true;
        lastInstantIdentity_ = identity;
        hasInstantIdentity_ = true;
        result.peerEffectVersion = ++buffVersion_;
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
            buffVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        if (!RestoreRecharges(context) || !RestoreBuffs(context))
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        auto nextLegends = legends_;
        if (!nextLegends.Medium(medium == 1, context, result))
            return WorldActionResult::Failed;
        if (medium == medium_) {
            // The first motion report also selects the weapon set. It is not
            // an environment change and must not interrupt an existing cast.
            if (!modern::WeaponSelection(298, 57344, medium, result.owned) ||
                !modern::WeaponSelection(298, peer_.character, medium, result.peers))
                return WorldActionResult::Failed;
            if (medium != 1) {
                auto remaining = buffs_;
                remaining.erase(
                    std::remove_if(remaining.begin(), remaining.end(),
                                   [](const TimedBuff& buff) { return buff.definition == kAquaticStance.buff; }),
                    remaining.end());
                if (remaining.size() != buffs_.size()) {
                    for (const auto& buff : buffs_)
                        if (buff.definition == kAquaticStance.buff && buff.announced &&
                            (!BuffRemove(57344, buff.instance, result.owned) ||
                             !BuffRemove(peer_.character, buff.instance, result.peers)))
                            return WorldActionResult::Failed;
                    if (!context.ReserveSequences(result.sequences) || !context.persistSkillState ||
                        !context.persistSkillState(
                            RechargeText(rechargeEndsUtc_),
                            hammers_.Text(BuffText(remaining, nextBuffInstance_), energy_, context)))
                        return Notice("The cancelled skill state could not be saved.", out);
                    buffs_ = std::move(remaining);
                }
            }
            legends_ = std::move(nextLegends);
            mediumReported_ = true;
            result.peerEffectVersion = ++buffVersion_;
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
        if (!modern::WeaponSelection(298, 57344, medium, result.owned) ||
            !modern::WeaponSelection(298, peer_.character, medium, result.peers))
            return WorldActionResult::Failed;
        std::vector<TimedBuff> remainingBuffs;
        for (const auto& buff : buffs_) {
            if (medium != 1 && buff.definition == kAquaticStance.buff) {
                if (buff.announced && (!BuffRemove(57344, buff.instance, result.owned) ||
                                       !BuffRemove(peer_.character, buff.instance, result.peers)))
                    return WorldActionResult::Failed;
            } else if (buff.announced || buff.startsAt <= context.now ||
                       (!mediumReported_ && !castEndsAt_ &&
                        (buff.definition == modern::GreatDwarfData<204489>::Buff ||
                         buff.definition == modern::BurstStrength::Buff)))
                // First observed medium after reconnect is not an interruption of the saved Rite.
                remainingBuffs.push_back(buff);
        }
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        const bool cancelledHammers = (mediumReported_ || castEndsAt_) && nextHammers.Present();
        if (cancelledHammers && !nextHammers.Stop(context, nextEnergy, result))
            return WorldActionResult::Failed;
        if (remainingBuffs.size() != buffs_.size() || cancelledHammers) {
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            if (!context.persistSkillState ||
                !context.persistSkillState(
                    RechargeText(rechargeEndsUtc_),
                    nextHammers.Text(BuffText(remainingBuffs, nextBuffInstance_), nextEnergy, context)))
                return cancelledHammers ? Done(out) : Notice("The cancelled skill state could not be saved.", out);
        }
        buffs_ = std::move(remainingBuffs);
        nextHammers.Saved();
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        pendingHeals_.erase(std::remove_if(pendingHeals_.begin(), pendingHeals_.end(),
                                           [&](const PendingHeal& heal) {
                                               return heal.startsAt > context.now || heal.skill == 13027 ||
                                                      heal.skill == 5834 || heal.skill == 13050;
                                           }),
                            pendingHeals_.end());
        phase_ = modern::PhasedSkill{};
        castEndsAt_ = 0;
        pendingRecharge_ = false;
        missiles_.CancelPending();
        brand_.CancelWarp();
        stealth_.CancelPending();
        absorptionAt_ = absorptionUtc_ = 0;
        approachAt_ = approachUtc_ = 0;
        backstepAt_ = backstepUtc_ = 0;
        greatDwarfAt_ = greatDwarfUtc_ = 0;
        dragonRelease_.Cancel();
        CancelAssassinInitialHeal();
        legends_ = std::move(nextLegends);
        medium_ = medium;
        mediumReported_ = true;
        result.peerEffectVersion = ++buffVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool InvalidStance(const TimedBuff& buff) const {
        if (buff.definition != kAquaticStance.buff)
            return false;
        if (mediumReported_ && medium_ != 1)
            return true;
        for (const auto& skill : skills_)
            if (skill.skill == kAquaticStance.skill)
                return skill.equipmentSlot >= definitions_.size() || definitions_[skill.equipmentSlot] != skill.weapon;
        return true;
    }
    bool ActiveStance(uint64_t now) const {
        for (const auto& buff : buffs_)
            if (buff.definition == kAquaticStance.buff && now >= buff.startsAt && now < buff.endsAt &&
                !InvalidStance(buff))
                return true;
        return false;
    }
    bool RestoreBuffs(const WorldActionContext& context) {
        if (effectsRestored_)
            return hammers_.ClockValid(context);
        auto nextHammers = hammers_;
        auto nextEnergy = energy_;
        if (!nextHammers.Restore(context, nextEnergy) || !nextHammers.ClockValid(context))
            return false;
        std::vector<TimedBuff> restored;
        bool expired = false;
        for (auto buff : buffs_) {
            if (!context.wallNow || context.peerAgent != peer_.character)
                return false;
            if (buff.endsUtc <= context.wallNow) {
                expired = true;
                continue;
            }
            unsigned activation = 0;
            for (size_t i = 0; i < selfBuffs_.size(); ++i)
                for (const auto& effect : selfBuffs_[i])
                    if (effect.definition == buff.definition && skills_[i].activationMs > activation)
                        activation = skills_[i].activationMs;
            const uint64_t remaining = buff.endsUtc - context.wallNow;
            const uint64_t preparation = buff.startsUtc > context.wallNow ? buff.startsUtc - context.wallNow : 0;
            if (preparation > activation || remaining > buff.endsUtc - buff.startsUtc + activation ||
                context.now > UINT64_MAX - remaining)
                return false;
            buff.startsAt = context.now + preparation;
            buff.endsAt = context.now + remaining;
            restored.push_back(buff);
        }
        if (nextHammers.NeedsSave()) {
            if (!context.persistSkillState ||
                !context.persistSkillState(
                    RechargeText(rechargeEndsUtc_),
                    nextHammers.Text(BuffText(restored, nextBuffInstance_), nextEnergy, context)))
                return false;
            nextHammers.Saved();
            expired = false;
        }
        hammers_ = std::move(nextHammers);
        energy_ = std::move(nextEnergy);
        buffs_ = std::move(restored);
        effectsRestored_ = true;
        effectsNeedSave_ = expired;
        return true;
    }
    bool PrepareBuffs(const WorldActionContext& context, WorldActionOutput& result, std::vector<TimedBuff>& next,
                      uint64_t heldRiteEnd = 0) const {
        if (buffVersion_ == UINT64_MAX)
            return false;
        for (auto buff : buffs_) {
            if (context.now >= buff.endsAt || InvalidStance(buff)) {
                if (buff.announced && (!BuffRemove(57344, buff.instance, result.owned) ||
                                       !BuffRemove(peer_.character, buff.instance, result.peers)))
                    return false;
                continue;
            }
            if (!buff.announced && context.now >= buff.startsAt &&
                !(heldRiteEnd && buff.definition == RiteAllies::Buff && buff.endsUtc == heldRiteEnd)) {
                if (!BuffCreate(57344, buff, context.now, level_, result.owned) ||
                    !BuffCreate(peer_.character, buff, context.now, level_, result.peers))
                    return false;
                buff.announced = true;
            }
            next.push_back(buff);
        }
        return true;
    }
    bool RestoreRecharges(const WorldActionContext& context) {
        if (rechargesRestored_)
            return true;
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
        if (!ground && slot >= 12 && slot <= 15) {
            if (legends_.Enabled())
                return legends_.Swap(skillId, slot, targetOrFrame, identity, context, level_, energy_, buffVersion_,
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
        if (!skill)
            return Notice("That skill is not available in this slot.", out);
        const auto stance = StanceForSkill(skillId), followup = StanceForFollowup(skillId);
        const bool brandBase = skillId == modern::RevenantBrandRules::Base,
                   brandFollowup = skillId == modern::RevenantBrandRules::Followup;
        if (brandFollowup && !brand_.Active(context))
            return Done(out);
        if (skillId == modern::StealthRules::Attack && !stealth_.Active(context.now))
            return Done(out);
        const bool selfStealth = skillId == modern::StealthRules::SelfSource, engineerHeal = skillId == 5834;
        const bool venom = skillId == modern::RevenantSphereRules::Base;
        const bool venomFollowup = skillId == modern::RevenantSphereRules::Followup;
        const bool detonate = skillId == 5653 || venomFollowup;
        const uint32_t sphereBase = venomFollowup ? modern::RevenantSphereRules::Base : 5652;
        if (detonate && (!missiles_.CanDetonate(context.now, sphereBase) || (skillId == 5653 && attunement_ != 3)))
            return Done(out);
        if (detonate && buffVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        if ((followup && !ActiveStance(context.now)) || (stance && ActiveStance(context.now)))
            return Done(out);
        // An explicit caster-only effect ignores the UI's selected target.
        const bool dwarfHeal = skillId == 27372, demonHeal = skillId == DemonHealingData::Skill;
        const bool assassinHeal = skillId == 26937, legendHeal = dwarfHeal || demonHeal || assassinHeal;
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
        if ((approach || backstep) && legends_.Active() != 16)
            return Notice("This skill is not available yet.", out);
        if (absorption && legends_.Active() != Absorption::Legend())
            return Notice("This skill is not available yet.", out);
        if (leap && legends_.Active() != 18)
            return Notice("This skill is not available yet.", out);
        if (maintained && legends_.Active() != Maintained::LegendFor(skillId))
            return Notice("This skill is not available yet.", out);
        if ((maintainedOn && hammers_.Present()) ||
            (maintainedOff &&
             (!hammers_.Active(context.now) || hammers_.CurrentSkill() != Maintained::StartFor(skillId))))
            return Done(out);
        InitialWeaponSkill ownSelfStealth;
        if ((selfStealth || engineerHeal) && skill) {
            ownSelfStealth = *skill;
            ownSelfStealth.recoveryMs = skill->animationMs - skill->activationMs;
            skill = &ownSelfStealth;
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
            ownAbsorption.timingKnown = true;
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
            ownJade.activationMs = combat::JadeWinds::Activation(204489, medium_ == 1);
            ownJade.animationMs = 2033;
            ownJade.recoveryMs = 2033 - ownJade.activationMs;
            skill = &ownJade;
        }
        InitialWeaponSkill ownEngagement;
        if (skillId == 26679 && skill) {
            ownEngagement = *skill;
            ownEngagement.activationMs = combat::ForcedEngagement::Activation(204489, medium_ == 1);
            ownEngagement.animationMs = combat::ForcedEngagement::Animation(204489, medium_ == 1);
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
            ownInspiring.timingKnown = true;
            ownInspiring.groundTargeted = true;
            ownInspiring.minimumGroundRange = 0;
            ownInspiring.maximumGroundRange = 900;
            skill = &ownInspiring;
        }
        const bool engagement = skillId == 26679, jade = skillId == 28406,
                   banish = skillId == combat::BanishEnchantment::Skill;
        const bool greatDwarf = skillId == modern::GreatDwarfData<204489>::Skill,
                   legendAction = engagement || jade || banish || legendHeal || greatDwarf || maintainedOn ||
                                  maintainedOff || absorption || inspiring || approach || backstep || leap || facet;
        const uint32_t target = (banish || engagement) ? targetOrFrame
                                : approach             ? targetOrFrame
                                : brandFollowup        ? brand_.Target()
                                : ground || selfStealth || engineerHeal || stance || followup || detonate || venom ||
                                        legendAction ||
                                        (!selfBuffs_[skillIndex].empty() && targetOrFrame != 57344 &&
                                         !(skill->requiresWeapon && skill->slotType == 3))
                                    ? 0
                                    : targetOrFrame;
        if (skill->requiresWeapon &&
            (skill->equipmentSlot >= definitions_.size() || definitions_[skill->equipmentSlot] != skill->weapon))
            return Notice("That weapon skill is not available with the current equipment.", out);
        const bool underwater = medium_ == 1;
        const unsigned environment = underwater ? 2u : 1u;
        if (skill->requiresWeapon ? skill->underwater != underwater : !(skill->supportEnvironments & environment))
            return Notice("That skill is not available in this environment.", out);
        // Audited base legend actions require their own active legend. Other conditional
        // entries retain their trait and state checks.
        if (!skill->timingKnown || skill->minimumLevel > level_ ||
            (!skill->requiresWeapon && !(skill->knownSupportConditions & environment) &&
             !(legendAction && legends_.Active() == (engagement ? combat::ForcedEngagement::Legend(204489)
                                                     : jade     ? combat::JadeWinds::Legend(204489)
                                                     : banish   ? combat::BanishEnchantment::Legend(204489)
                                                     : facet    ? facetRule.legend
                                                     : leap     ? 18
                                                     : (approach || backstep) ? 16
                                                     : maintained             ? Maintained::LegendFor(skillId)
                                                     : absorption             ? Absorption::Legend()
                                                     : assassinHeal           ? 16
                                                     : demonHeal              ? DemonHealingData::Legend
                                                                              : 17))))
            return Notice("This skill is not available yet.", out);
        if (banish || engagement) {
            WorldCombatTarget enemy;
            if (!target || !context.combatTarget || !context.combatTarget(target, enemy))
                return Notice("Choose a combat opponent.", out);
            const auto& p = enemy.actor.position;
            const double distance =
                32 * std::hypot(std::hypot(double(p.x) - context.position.x, double(p.y) - context.position.y),
                                double(p.z) - context.position.z);
            if (!std::isfinite(distance) || distance > (engagement ? combat::ForcedEngagement::Range(204489)
                                                                   : combat::BanishEnchantment::Range(204489)))
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
        if (target && target != 57344 &&
            (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(target)))
            return Notice("That target is no longer in this map.", out);
        if (skill->variableStages && skill->phases.empty())
            return Notice("This skill is not available yet.", out);
        if (skill->requiresWeapon && (skill->underwater || skill->maximumGroundRange > 0) &&
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
        // These stationary legend actions use the aquatic animation without a terrestrial step. Start its
        // stationary aquatic action once, then close at the ability's cast time.
        // Replaying land step 1 triggers MvCliContext's stepCount assertion.
        const bool phased = skill->variableStages || (underwater && !instant && !legendAction);
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
        const uint64_t duration = static_cast<uint64_t>(skill->activationMs) + skill->recoveryMs;
        const uint64_t rechargeDuration = static_cast<uint64_t>(skill->activationMs) + skill->rechargeMs;
        if (context.now > UINT64_MAX - duration || context.now > UINT64_MAX - rechargeDuration)
            return WorldActionResult::Failed;
        const int32_t selfHeal = selfHeals_[skillIndex];
        if (selfHeal || greatDwarf || maintainedOn || absorption || approach || backstep || leap || facet ||
            selfStealth) {
            float health = 0;
            if (!ReadHealthState(context.healthState, health) || (selfHeal && pendingHeals_.size() >= 64))
                return WorldActionResult::Failed;
            if (health == 0)
                return Notice("This skill cannot be used while defeated.", out);
        }
        const auto& effects = selfBuffs_[skillIndex];
        uint32_t requestedNext = nextBuffInstance_;
        std::vector<TimedBuff> requestedBuffs = buffs_;
        for (const auto& effect : effects) {
            if (selfStealth || engineerHeal)
                continue; // Apply own benefits with the heal at its atomic trigger, not at acceptance.
            const unsigned delay = stance ? 0 : skill->activationMs;
            const uint64_t starts = context.now + delay;
            if (requestedBuffs.size() >= 64 || requestedNext >= modern::kFirstConditionInstance || level_ > UINT8_MAX ||
                starts > UINT64_MAX - effect.durationMs || !context.wallNow || context.wallNow > UINT64_MAX - delay ||
                context.wallNow + delay > UINT64_MAX - effect.durationMs)
                return WorldActionResult::Failed;
            TimedBuff buff;
            buff.definition = effect.definition;
            buff.instance = requestedNext++;
            buff.startsAt = starts;
            buff.endsAt = starts + effect.durationMs;
            buff.startsUtc = context.wallNow + delay;
            buff.endsUtc = buff.startsUtc + effect.durationMs;
            requestedBuffs.push_back(buff);
        }
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
        const bool finishBeforeStart = interrupt || (!phase_.Active() && castEndsAt_ && context.now >= castEndsAt_);
        if (finishBeforeStart && context.sequence >= UINT32_MAX - 1)
            return WorldActionResult::Failed;
        const uint32_t startSequence = context.sequence + (finishBeforeStart ? 1u : 0u);
        WorldActionOutput result;
        result.sequences = finishBeforeStart ? 2 : 1;
        // A request can arrive before the next Tick. Close the previous skill
        // first so it cannot be silently replaced without its final notice.
        if (finishBeforeStart && (!SkillEnd(57344, context.sequence, lastSkillIdentity_, result.owned) ||
                                  !SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.peers)))
            return WorldActionResult::Failed;
        // The behavior prefix identifies the client's predicted order. Native
        // BeginOrder and CompleteOrder match it exactly, including the high bit.
        // It is independent of the client tick and the server's deadline clock.
        if (!instant && pendingRecharge_ && !PendingRecharge(context, result.owned))
            return WorldActionResult::Failed;
        modern::PhasedSkill nextPhase;
        if (phased) {
            if (buffVersion_ == UINT64_MAX)
                return WorldActionResult::Failed;
            if (target == 57344 && (skill->requiresWeapon || skill->variableStages))
                return Notice("Choose another target for this skill.", out);
            if (finishBeforeStart && target &&
                !SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.targetView))
                return WorldActionResult::Failed;
            if (!nextPhase.Begin(skill->phases, static_cast<unsigned>(duration), skillId, slot, identity, target,
                                 underwater, skill->minimumGroundRange,
                                 skill->requiresWeapon && (skill->underwater || skill->maximumGroundRange > 0)
                                     ? skill->maximumGroundRange
                                     : -1) ||
                !nextPhase.Emit(approach ? ApproachPhaseContext(context) : context, startSequence, result, true))
                return WorldActionResult::Failed;
        }
        const uint32_t peerTarget = target == 57344 ? peer_.character : target;
        if (elite || absorption || inspiring || facetOn || facetRelease || facetBurst || assassinHeal || selfStealth) {
            modern::SkillStage stage;
            stage.index = 0;
            stage.durationMs = static_cast<uint16_t>(skill->animationMs);
            if (!modern::SkillPhase(57344, startSequence, identity, skillId, slot, 0, stage, true, false, nullptr,
                                    result.owned, underwater) ||
                !modern::SkillPhase(peer_.character, startSequence, identity, skillId, slot, 0, stage, true, false,
                                    nullptr, result.peers, underwater))
                return WorldActionResult::Failed;
        } else if (!phased && (!SkillBegin(57344, startSequence, identity, skillId, slot, skill->activationMs, target,
                                           result.owned, underwater) ||
                               !SkillBegin(peer_.character, startSequence, identity, skillId, slot, skill->activationMs,
                                           peerTarget, result.peers, underwater)))
            return WorldActionResult::Failed;
        if (target && target != 57344 && !phased) {
            result.targetPeerAgent = static_cast<uint16_t>(target);
            if ((finishBeforeStart &&
                 !SkillEnd(peer_.character, context.sequence, lastSkillIdentity_, result.targetView)) ||
                !SkillBegin(peer_.character, startSequence, identity, skillId, slot, skill->activationMs, 57344,
                            result.targetView, underwater))
                return WorldActionResult::Failed;
        }
        if (instant && skill->rechargeMs && !SkillRecharge(skillId, skill->rechargeMs, skill->rechargeMs, result.owned))
            return WorldActionResult::Failed;
        if (stance) {
            auto& buff = requestedBuffs.back();
            if (buffVersion_ == UINT64_MAX || !BuffCreate(57344, buff, context.now, level_, result.owned) ||
                !BuffCreate(peer_.character, buff, context.now, level_, result.peers))
                return WorldActionResult::Failed;
            buff.announced = true;
        }
        if (followup) {
            if (buffVersion_ == UINT64_MAX)
                return WorldActionResult::Failed;
            for (const auto& buff : requestedBuffs)
                if (buff.definition == followup->buff && buff.announced &&
                    (!BuffRemove(57344, buff.instance, result.owned) ||
                     !BuffRemove(peer_.character, buff.instance, result.peers)))
                    return WorldActionResult::Failed;
            requestedBuffs.erase(
                std::remove_if(requestedBuffs.begin(), requestedBuffs.end(),
                               [&](const TimedBuff& buff) { return buff.definition == followup->buff; }),
                requestedBuffs.end());
        }
        // Save before publishing the action. Failed storage consumes neither
        // the predicted identity nor the skill, and cannot bypass a recharge.
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
                                   skill->maximumGroundRange, target, context, level_, 204489,
                                   brandBase ? &projectile
                                   : venom   ? &sphere
                                             : nullptr) ||
            (detonate && !nextMissiles.Detonate(204489, context, result, sphereBase)))
            return WorldActionResult::Failed;
        auto nextHammers = hammers_;
        const bool saveEffects = !effects.empty() || !buffs_.empty() || nextBuffInstance_ != 1 || effectsNeedSave_ ||
                                 maintainedOn || absorption || inspiring || approach || backstep || leap || facet ||
                                 nextHammers.Tracked();
        bool hammersChanged = false;
        if (nextHammers.Tracked() &&
            (!nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, result, hammersChanged) ||
             (hammersChanged && buffVersion_ == UINT64_MAX)))
            return WorldActionResult::Failed;
        if (!maintainedOff && !nextEnergy.Spend(skillId, context.now, result))
            return WorldActionResult::Failed;
        if (facetOn) {
            if (buffVersion_ == UINT64_MAX || requestedNext >= modern::kFirstConditionInstance ||
                !nextHammers.ScheduleFacet(facetRule.base, requestedNext++, context, nextEnergy, medium_ == 1))
                return WorldActionResult::Failed;
        }
        if (maintainedOn) {
            if (buffVersion_ == UINT64_MAX || requestedNext >= modern::kFirstConditionInstance ||
                !nextHammers.Schedule(requestedNext++, context, nextEnergy, skillId, medium_ == 1))
                return WorldActionResult::Failed;
        }
        // Zero authored animation: apply the maintained marker in the same
        // candidate transaction as its initial energy cost, before publishing.
        if (assassin && maintainedOn &&
            (!nextHammers.Advance(context, legends_.Active(), level_, nextEnergy, result, hammersChanged) ||
             !nextEnergy.Advance(context.now, result)))
            return WorldActionResult::Failed;
        if ((facetRelease || facetBurst || facetGaze || facetElements) &&
            (buffVersion_ == UINT64_MAX || !nextHammers.ConsumeFacet(facetRule.base, context, nextEnergy, result) ||
             !nextEnergy.Advance(context.now, result)))
            return WorldActionResult::Failed;
        float facetPrevious = 0, facetCurrent = 0;
        std::string facetHealth;
        if (facetHeal) {
            if (buffVersion_ == UINT64_MAX || !modern::ReadHealthState(context.healthState, facetPrevious) ||
                facetPrevious <= 0 || !nextHammers.ConsumeFacet(facetRule.base, context, nextEnergy, result) ||
                !nextEnergy.Advance(context.now, result))
                return WorldActionResult::Failed;
            // Own healing model, current profile healing attribute6 is zero.
            facetCurrent =
                (std::min)(modern::kPublishedMaximumHealth,
                           facetPrevious + 125.0f + static_cast<float>(std::round(double(level_) * level_ * .27)));
            const float delta = facetCurrent - facetPrevious;
            if (!modern::WriteHealthState(facetCurrent, facetHealth) ||
                (delta > 0 && (!modern::HealthChange<204489>(57344, 27228, delta, result.owned) ||
                               !modern::HealthChange<204489>(peer_.character, 27228, delta, result.peers))))
                return WorldActionResult::Failed;
        }
        if (maintainedOff && (buffVersion_ == UINT64_MAX || !nextHammers.Stop(context, nextEnergy, result)))
            return WorldActionResult::Failed;
        modern::InitiativeLedger initiative;
        const bool initiativeEnabled = bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return WorldActionResult::Failed;
        if (initiativeEnabled) {
            unsigned cost = 0;
            if ((skill->requiresWeapon && !modern::InitialInitiativeCost(204489, skillId, cost)) ||
                !initiative.Load(context.initiativeState) || !initiative.Advance(context.wallNow))
                return WorldActionResult::Failed;
            if (!initiative.Spend(cost))
                return Done(out);
            if (!modern::InitiativeCurrent(204489, 57344, initiative, result.owned))
                return WorldActionResult::Failed;
        }
        if ((initiativeEnabled || saveEffects || skill->rechargeMs) && !context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (initiativeEnabled || saveEffects) {
            const auto persistedEffects =
                nextHammers.Text(BuffText(requestedBuffs, requestedNext), nextEnergy, context);
            const auto hostile = facetGaze ? combat::GazeDarkness::Commit(204489, context, RechargeText(savedRecharges),
                                                                          persistedEffects)
                                           : WorldConditionCommit::Applied;
            if (hostile == WorldConditionCommit::RecoveryRequired)
                return WorldActionResult::Failed;
            const bool stored =
                facetGaze ? hostile == WorldConditionCommit::Applied
                : initiativeEnabled
                    ? context.persistCombatInitiative(RechargeText(savedRecharges), persistedEffects,
                                                      context.healthState, context.conditionState, initiative.Text())
                : facetHeal ? (context.persistCombatConditions &&
                               context.persistCombatConditions(RechargeText(savedRecharges), persistedEffects,
                                                               facetHealth, context.conditionState))
                            : (backstep && (context.stunned || context.taunted)
                                   ? (context.persistStunBreakState &&
                                      context.persistStunBreakState(RechargeText(savedRecharges), persistedEffects))
                                   : (context.persistSkillState &&
                                      context.persistSkillState(RechargeText(savedRecharges), persistedEffects)));
            if (!stored)
                return initiativeEnabled ? Done(out)
                       : maintainedOn || maintainedOff || absorption || inspiring || approach || backstep || leap ||
                               facet
                           ? Done(out)
                           : Notice("The skill state could not be saved.", out);
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
        if (!effects.empty() || followup)
            buffs_ = std::move(requestedBuffs);
        nextBuffInstance_ = requestedNext;
        if (selfHeal) {
            PendingHeal heal;
            heal.startsAt = context.now + skill->activationMs;
            heal.skill = skillId;
            heal.amount = selfHeal;
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
        effectsNeedSave_ = false;
        if (instant) {
            if (finishBeforeStart)
                castEndsAt_ = 0;
            lastInstantIdentity_ = identity;
            hasInstantIdentity_ = true;
        } else {
            phase_ = std::move(nextPhase);
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
            if (beta::MeleeCombatSkill(204489, skillId) && (banish || skill->requiresWeapon) && !skill->rechargeMs &&
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
                        melee_.maximum = float(combat::BanishEnchantment::Range(204489));
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
                const auto activation = combat::JadeWinds::Activation(204489, medium_ == 1);
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
                    const auto event = combat::ForcedEngagement::Activation(204489, medium_ == 1);
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
            melee_.settlesAt = context.now + combat::ElementalBlast::Activation(204489, medium_ == 1);
            melee_.impactUtc = context.wallNow + combat::ElementalBlast::Activation(204489, medium_ == 1);
        }
        if (facetBurst) {
            melee_ = {};
            melee_.skill = 28113;
            melee_.identity = identity;
            melee_.medium = medium_;
            melee_.settlesAt = context.now + combat::BurstStrengthImpact::Activation(204489, medium_ == 1);
            melee_.impactUtc = context.wallNow + combat::BurstStrengthImpact::Activation(204489, medium_ == 1);
        }
        if (phased || stance || followup || detonate || maintainedOff || facetHeal || facetRelease || facetBurst ||
            facetGaze || facetElements || hammersChanged)
            result.peerEffectVersion = ++buffVersion_;
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
                   legends_.Active() != combat::ForcedEngagement::Legend(204489) || now >= melee_.settlesAt;
        if (melee_.skill == 27917)
            return !phase_.Active() || phase_.skill != 27917 || lastSkillIdentity_ != melee_.identity ||
                   medium_ != melee_.medium || legends_.Active() != combat::DemonLeapImpact::Legend(204489) ||
                   now >= melee_.settlesAt;
        if (melee_.skill == 27162)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::ElementalBlast::Legend(204489) || now >= melee_.settlesAt;
        if (melee_.skill == 28113)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::BurstStrengthImpact::Legend(204489) || now >= melee_.settlesAt;
        if (melee_.skill == 28406)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::JadeWinds::Legend(204489) || now >= melee_.settlesAt;
        if (melee_.skill == 27505)
            return lastSkillIdentity_ != melee_.identity || medium_ != melee_.medium ||
                   legends_.Active() != combat::BanishEnchantment::Legend(204489) || now >= melee_.settlesAt;
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
                legends_.Active() != combat::ForcedEngagement::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
                medium_ != melee_.medium || legends_.Active() != combat::DemonLeapImpact::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
                legends_.Active() != combat::ElementalBlast::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
                legends_.Active() != combat::BurstStrengthImpact::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
                legends_.Active() != combat::JadeWinds::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
                legends_.Active() != combat::BanishEnchantment::Legend(204489)) {
                melee_ = {};
                return WorldActionResult::Handled;
            }
            auto contact = melee_;
            const int hit = contact.Settle(204489, peer_.character, level_, c, out);
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
        const int hit = contact.Settle(204489, peer_.character, level_, c, out, &end);
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
        if (!Build204489Game().ServiceChat(text, sender, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    PeerIdentity peer_;
    unsigned level_;
    std::vector<InitialWeaponSkill> skills_;
    std::array<uint32_t, 68> definitions_{};
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
    modern::RevenantBrand<204489> brand_;
    modern::Stealth<204489> stealth_;
    modern::Energy<204489> energy_;
    modern::LegendActions<204489> legends_;
    modern::Hammers<204489> hammers_;
    uint64_t absorptionAt_ = 0, absorptionUtc_ = 0;
    bool absorptionWater_ = false, absorptionRecoveryRequired_ = false;
    uint8_t medium_ = 0;
    bool mediumReported_ = false;
    uint32_t lastInstantIdentity_ = 0;
    bool hasInstantIdentity_ = false;
    std::vector<uint64_t> rechargeEndsAt_;
    bool pendingRecharge_ = false;
    size_t pendingRechargeIndex_ = 0;
    uint64_t rechargeStartsAt_ = 0;
    std::vector<uint64_t> rechargeEndsUtc_;
    std::vector<bool> resumeReports_;
    bool rechargesRestored_ = false;
    std::vector<SelfBuffs> selfBuffs_;
    std::vector<TimedBuff> buffs_;
    modern::ConditionActions<204489> conditions_;
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
    uint32_t nextBuffInstance_ = 1;
    uint64_t buffVersion_ = 0;
    bool effectsRestored_ = false, effectsNeedSave_ = false;
    std::vector<int32_t> selfHeals_;
    modern::Regeneration regeneration_;
    std::vector<PendingHeal> pendingHeals_;
    uint8_t attunement_ = 0;
    bool attunementReported_ = false;
};
} // namespace
bool CreateWeaponController(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WeaponController>& out, std::string& error) {
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
            error = "Invalid saved Build 204489 endurance";
            return false;
        }
    }
    PeerIdentity expected;
    if (peer.player < 2 || !Build204489Bootstrap().PeerSlot(peer.player - 2, expected) ||
        peer.character != expected.character || peer.firstBag != expected.firstBag ||
        peer.itemBase != expected.itemBase) {
        error = "Invalid Build 204489 weapon actor reservation";
        return false;
    }
    InitialEquipment equipment;
    const std::vector<InitialWeaponSkill>*weapon = nullptr, *support = nullptr;
    modern::ConditionState<204489> conditionState;
    const auto conditionField = saved.profileFields.find("condition_effects");
    if (!conditionState.Load(conditionField == saved.profileFields.end() ? std::string() : conditionField->second)) {
        error = "Invalid saved Build 204489 conditions";
        return false;
    }
    uint8_t attunement = 0;
    if (!SavedAttunement(saved, attunement)) {
        error = "Invalid saved Build 204489 elemental attunement";
        return false;
    }
    if (!catalog.Resolve(saved, equipment) || !catalog.ResolveInitialWeaponSkills(saved, weapon) ||
        !catalog.ResolveInitialSupportSkills(saved, support)) {
        error = "Cannot resolve original Build 204489 weapon actions";
        return false;
    }
    std::vector<InitialWeaponSkill> skills = *weapon;
    skills.insert(skills.end(), support->begin(), support->end());
    const std::vector<InitialWeaponSkill>* aquatic = nullptr;
    if (!catalog.ResolveAquaticWeaponSkills(saved, aquatic))
        return false;
    skills.insert(skills.end(), aquatic->begin(), aquatic->end());
    for (auto& skill : skills) {
        if (skill.skill == Approach::Skill)
            ConfigureApproach(skill, false);
        if (skill.skill == Backstep::Skill)
            ConfigureBackstep(skill, false);
        if (skill.skill == DemonLeap::Skill)
            ConfigureDemonLeap(skill, false);
    }
    std::set<uint32_t> identities;
    for (const auto& skill : skills)
        if (!identities.insert(skill.skill).second) {
            error = "Conflicting original Build 204489 skill action identities";
            return false;
        }
    std::vector<uint64_t> recharges;
    if (!LoadSkillRecharges(saved, skills, recharges, error))
        return false;
    float currentHealth = 0;
    if (!ReadHealthState(saved, currentHealth)) {
        error = "Invalid saved Build 204489 health state";
        return false;
    }
    InventoryState inventory;
    if (!catalog.Inventory(saved, inventory, error))
        return false;
    for (size_t slot = 0; slot < 68; ++slot)
        if (inventory.locations[slot] != -1)
            equipment.definitions[slot] = 0;
    std::vector<SelfBuffs> selfBuffs(skills.size());
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == kAirAuraSkill) {
            selfBuffs[i].emplace_back(kAirAuraBuff, kAirAuraDurationMs);
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == modern::DwarfEffectRules::Skill) {
            selfBuffs[i].emplace_back(modern::DwarfEffectRules::Buff, modern::DwarfEffectRules::DurationMs);
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == modern::GreatDwarfData<204489>::Skill) {
            // Only the unconditional own base fact; conditional80245 is not a universal effect.
            selfBuffs[i].emplace_back(modern::GreatDwarfData<204489>::Buff, modern::GreatDwarfData<204489>::DurationMs);
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (const auto stance = StanceForSkill(skills[i].skill)) {
            selfBuffs[i].emplace_back(stance->buff, stance->durationMs);
        }
    std::vector<int32_t> selfHeals(skills.size(), 0);
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == 5503 || skills[i].skill == 14389 || skills[i].skill == 14401 ||
            skills[i].skill == 9102 || skills[i].skill == 5857 || skills[i].skill == 27372) {
            // Explicit single-target active self heals. Other descriptions can be
            // pulses or conditional values and must not become guessed actions.
            // Shelter's blocking and Elixir H's boons require separate effect rules.
            const std::vector<InitialSkillQuantity>* quantities = nullptr;
            const InitialSkillQuantity* selected = nullptr;
            if (!catalog.ResolveInitialSupportQuantities(saved, skills[i].skill, quantities) || !quantities) {
                error = "Cannot resolve original Build 204489 active healing amount";
                return false;
            }
            for (const auto& quantity : *quantities)
                if (quantity.group == 0 && quantity.metadata == 34 &&
                    quantity.mode == (skills[i].skill == 27372 ? 0u : 4u) && quantity.modeFlags == 0 &&
                    !quantity.requiresTrait && !quantity.requiresPredicate && quantity.attribute == 6 &&
                    quantity.count == 1) {
                    if (selected) {
                        error = "Ambiguous original Build 204489 active healing amount";
                        return false;
                    }
                    selected = &quantity;
                }
            // Accepted bootstrap publishes healing attribute 6 as zero. Full stats
            // and combat modifiers remain separate requirements.
            if (!selected || !selected->Amount(saved.level, 0, selfHeals[i]) || selfHeals[i] <= 0) {
                error = "Invalid original Build 204489 active healing amount";
                return false;
            }
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == 13027) {
            // Own default fact: mode4/flags1 is admitted only for this exact model;
            // trait+predicate refs are null. Do not relax the general healing filter.
            const std::vector<InitialSkillQuantity>* quantities = nullptr;
            const InitialSkillQuantity* selected = nullptr;
            if (!catalog.ResolveInitialSupportQuantities(saved, 13027, quantities) || !quantities)
                return false;
            for (const auto& q : *quantities)
                if (q.group == 0 && q.metadata == 34 && q.mode == 4 && q.modeFlags == 1 && !q.requiresTrait &&
                    !q.requiresPredicate && q.attribute == 6 && q.count == 1 && q.base == 138 &&
                    q.attributeMultiplier == 1 && q.levelExponent == 2 && q.levelMultiplier == .92f) {
                    if (selected) {
                        error = "Ambiguous own13027 healing model";
                        return false;
                    }
                    selected = &q;
                }
            if (!selected || !selected->Amount(saved.level, 0, selfHeals[i]) || selfHeals[i] <= 0) {
                error = "Missing own13027 healing model";
                return false;
            }
            selfBuffs[i].emplace_back(718, 6000); // Own default fact, independent of received facet duration.
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == 13050) {
            // Explicit compatibility choice: use the exact own active model only.
            // The other own description is not an automatic or on-hit heal here.
            const std::vector<InitialSkillQuantity>* quantities = nullptr;
            const InitialSkillQuantity* selected = nullptr;
            if (!catalog.ResolveInitialSupportQuantities(saved, 13050, quantities) || !quantities)
                return false;
            for (const auto& q : *quantities)
                if (q.group == 0 && q.metadata == 34 && q.mode == 4 && q.modeFlags == 0 && !q.requiresTrait &&
                    !q.requiresPredicate && q.attribute == 6 && q.count == 1 && q.base == 75 &&
                    q.attributeMultiplier == .5f && q.levelExponent == 2 && q.levelMultiplier == .5f) {
                    if (selected) {
                        error = "Ambiguous own13050 active model";
                        return false;
                    }
                    selected = &q;
                }
            if (!selected || !selected->Amount(saved.level, 0, selfHeals[i]) || selfHeals[i] <= 0) {
                error = "Missing own13050 active model";
                return false;
            }
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == 5834) {
            // Own default model only; longer boon facts require a trait and are excluded.
            const std::vector<InitialSkillQuantity>* quantities = nullptr;
            const InitialSkillQuantity* selected = nullptr;
            if (!catalog.ResolveInitialSupportQuantities(saved, 5834, quantities) || !quantities)
                return false;
            for (const auto& q : *quantities)
                if (q.group == 0 && q.metadata == 34 && q.mode == 4 && q.modeFlags == 0 && !q.requiresTrait &&
                    !q.requiresPredicate && q.attribute == 6 && q.count == 1 && q.base == 120 &&
                    q.attributeMultiplier == 1 && q.levelExponent == 2 && q.levelMultiplier == .85f) {
                    if (selected) {
                        error = "Ambiguous own5834 healing model";
                        return false;
                    }
                    selected = &q;
                }
            if (!selected || !selected->Amount(saved.level, 0, selfHeals[i]) || selfHeals[i] <= 0) {
                error = "Missing own5834 healing model";
                return false;
            }
            selfBuffs[i].emplace_back(717, 2000);
            selfBuffs[i].emplace_back(718, 4000);
            selfBuffs[i].emplace_back(719, 4000);
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == 26937) {
            // The two own healing descriptions share metadata34. Select the
            // initial model by its full coefficients, never by metadata alone.
            const std::vector<InitialSkillQuantity>* quantities = nullptr;
            const InitialSkillQuantity* selected = nullptr;
            if (!catalog.ResolveInitialSupportQuantities(saved, 26937, quantities) || !quantities)
                return false;
            for (const auto& q : *quantities)
                if (q.group == 0 && q.metadata == 34 && q.mode == 0 && q.modeFlags == 0 && !q.requiresTrait &&
                    !q.requiresPredicate && q.attribute == 6 && q.count == 1 && q.base == 40 &&
                    q.attributeMultiplier == .25f && q.levelExponent == 2 && q.levelMultiplier == .2375f) {
                    if (selected) {
                        error = "Ambiguous own assassin initial heal";
                        return false;
                    }
                    selected = &q;
                }
            if (!selected || !selected->Amount(saved.level, 0, selfHeals[i]) || selfHeals[i] <= 0) {
                error = "Missing own assassin initial heal";
                return false;
            }
        }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == DemonHealingData::Skill) {
            selfHeals[i] = DemonHealingData::Base(saved.level);
            selfBuffs[i].emplace_back(DemonHealingData::Might, DemonHealingData::MightDurationMs);
            selfBuffs[i].emplace_back(DemonHealingData::DarkAura, DemonHealingData::DarkAuraDurationMs);
        }
    struct SelfBuffRule {
        uint32_t skill, buff;
        const char* duration;
        uint32_t durationNamespace;
    };
    const SelfBuffRule rules[] = {{12516, 12554, "Duration", 487115272u},
                                  {14392, 787, "Buff Duration", 2661187832u},
                                  {14406, 14453, "Duration", 487115272u},
                                  {14412, 1122, "Duration", 487115272u}};
    for (size_t i = 0; i < skills.size(); ++i)
        for (const auto& rule : rules)
            if (skills[i].skill == rule.skill) {
                // Explicit default self effects from the original dictionaries. Keep
                // Buff Pet, trait overrides and area/pulse effects separate.
                const auto* reference = catalog.FindInitialSupportParameter(saved, rule.skill, "Buff", 2435345739u);
                const auto* duration =
                    catalog.FindInitialSupportParameter(saved, rule.skill, rule.duration, rule.durationNamespace);
                const std::vector<InitialSkillParameter>* related = nullptr;
                uint32_t type = 0, id = 0;
                int64_t milliseconds = 0;
                if (!reference || !duration || !reference->Definition(type, id) || type != 64 || id != rule.buff ||
                    !duration->ScaledInteger(milliseconds) || milliseconds <= 0 || milliseconds > UINT32_MAX ||
                    !catalog.ResolveRelatedSupportParameters(saved, rule.skill, id, related) || !related ||
                    saved.level > UINT8_MAX) {
                    error = "Cannot resolve original Build 204489 self effect";
                    return false;
                }
                selfBuffs[i].emplace_back(id, static_cast<uint32_t>(milliseconds));
            }
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == modern::BurstStrength::Skill)
            selfBuffs[i].emplace_back(modern::BurstStrength::Buff, modern::BurstStrength::Duration);
    for (size_t i = 0; i < skills.size(); ++i)
        if (skills[i].skill == combat::InfuseLight::Skill)
            selfBuffs[i].emplace_back(combat::InfuseLight::Buff, combat::InfuseLight::Duration(204489));
    std::vector<TimedBuff> savedBuffs;
    uint32_t nextBuffInstance = 1;
    modern::Hammers<204489> hammers;
    Character legacyEffects;
    if (!hammers.Load(saved, legacyEffects)) {
        error = "Invalid saved Build 204489 maintained skill";
        return false;
    }
    modern::Regeneration regeneration;
    if (!LoadBuffs(legacyEffects, selfBuffs,
                   std::any_of(skills.begin(), skills.end(),
                               [](const InitialWeaponSkill& skill) { return skill.skill == Absorption::Skill; }),
                   savedBuffs, nextBuffInstance, regeneration, error))
        return false;
    modern::LegendActions<204489> legends;
    if (!legends.Load(saved)) {
        error = "Invalid saved Build 204489 legends";
        return false;
    }
    std::unique_ptr<WeaponController> next(
        new WeaponActions(peer, std::move(skills), equipment.definitions, std::move(recharges), saved.level,
                          std::move(selfBuffs), std::move(savedBuffs), nextBuffInstance, std::move(selfHeals),
                          std::move(regeneration), attunement, std::move(legends), std::move(hammers)));
    out = std::move(next);
    error.clear();
    return true;
}
bool CreateWeaponActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                         std::unique_ptr<WorldActions>& out, std::string& error) {
    std::unique_ptr<WeaponController> next;
    if (!CreateWeaponController(catalog, saved, peer, next, error))
        return false;
    out = std::move(next);
    return true;
}
} // namespace build204489
} // namespace glide
