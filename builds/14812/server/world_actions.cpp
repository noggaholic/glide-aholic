#include "world_actions.h"
#include "glide/profile/beta_dodge_evasion.h"
#include "glide/profile/beta_projectile_combat.h"
#include "weapon_projectile_fallbacks.h"
#include "thief_stance.h"
#include "glide/profile/beta_symbol_execution.h"
#include "glide/profile/beta_rock_throw.h"
#include "glide/profile/beta_phoenix_return.h"
#include "motion.h"
#include "motion_state.h"
#include "world_entry.h"
#include "inventory_state.h"
#include "glide/profile/beta_missile.h"
#include "glide/profile/missile_launch.h"
#include "glide/profile/july_water_ground_flight.h"
#include "glide/profile/beta_water_healing.h"
#include "glide/profile/beta_judgment_healing.h"
#include "glide/profile/beta_melee_combat.h"
#include "glide/profile/beta_siren_contact.h"
#include "glide/profile/initiative.h"
#include <algorithm>
#include <cmath>
#include <cstring>
namespace glide {
namespace july14812 {
uint16_t MissileAgent(uint16_t agent) {
    return beta::MissileAgent(agent);
}
namespace {
bool Deadline(uint64_t now, uint32_t delay, uint64_t& out) {
    if (now > UINT64_MAX - delay)
        return false;
    out = now + delay;
    return true;
}
struct Cursor {
    uint32_t first, used = 0;
    bool Next(uint32_t& out) {
        if (!first || used >= 32 || first >= UINT32_MAX - used)
            return false;
        out = first + used++;
        return true;
    }
};
struct ActiveSkill {
    SkillPlan plan;
    uint32_t identity = 0, target = 0;
    unsigned next = 0;
    uint64_t at = 0;
    std::array<float, 3> facing{};
    SkillMovement emittedMovement;
    bool emittedMoving = false;
};
struct Recharge {
    uint32_t skill = 0;
    uint64_t ready = 0, utc = 0;
};
uint32_t RechargeDuration(uint32_t skill, const june14505::SkillCatalog& catalog, uint32_t loadout,
                          uint32_t* casting = nullptr) {
    if (catalog.Candidates().empty()) {
        if (casting)
            *casting = skill == 6221 ? 2067 : 1500;
        return skill == 6221 ? 6000 : skill == 5258 ? 20000 : 0;
    }
    for (const auto& candidate : catalog.Candidates())
        if (candidate.loadout == loadout && candidate.skill == skill &&
            (candidate.weaponSlot == 29 || candidate.weaponSlot == 24)) {
            std::vector<june14505::SkillAnimationPhase> phases;
            std::string error;
            if (!june14505::BuildSkillPlan(candidate, phases, error))
                return 0;
            uint32_t duration = 0;
            for (const auto& phase : phases)
                duration += phase.duration;
            if (casting)
                *casting = duration;
            return candidate.recharge;
        }
    return 0;
}
bool Decimal(const std::string& text, uint64_t& out) {
    if (text.empty())
        return false;
    uint64_t value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9' || value > (UINT64_MAX - (ch - '0')) / 10)
            return false;
        value = value * 10 + (ch - '0');
    }
    out = value;
    return true;
}
bool SavedRecharges(const Character& saved, std::array<Recharge, MaximumWeaponRecharges>& out,
                    const june14505::SkillCatalog& catalog, uint32_t loadout) {
    const auto at = saved.profileFields.find("skill_recharges");
    if (at == saved.profileFields.end() || at->second.empty())
        return true;
    const auto& text = at->second;
    if (text.size() > MaximumWeaponRecharges * 32)
        return false;
    size_t begin = 0, index = 0;
    while (begin < text.size()) {
        const auto comma = text.find(',', begin), colon = text.find(':', begin),
                   end = comma == std::string::npos ? text.size() : comma;
        uint64_t id = 0, utc = 0;
        if (index >= out.size() || colon == std::string::npos || colon >= end ||
            !Decimal(text.substr(begin, colon - begin), id) || id > UINT32_MAX ||
            !RechargeDuration(static_cast<uint32_t>(id), catalog, loadout) ||
            !Decimal(text.substr(colon + 1, end - colon - 1), utc) || !utc)
            return false;
        for (size_t i = 0; i < index; ++i)
            if (out[i].skill == id)
                return false;
        out[index++] = {static_cast<uint32_t>(id), 0, utc};
        if (comma == std::string::npos)
            break;
        begin = comma + 1;
        if (begin == text.size())
            return false;
    }
    return true;
}
using Projectile = beta::MissileFlight;
class JulyActions final : public WeaponActions {
  public:
    JulyActions(uint16_t agent, bool pilot) : agent_(agent), pilot_(pilot), conditions_(agent) {}
    void SetCatalog(const june14505::SkillCatalog& catalog, uint32_t loadout) {
        catalog_ = catalog;
        loadout_ = loadout;
    }
    void LoadRecharges(const std::array<Recharge, MaximumWeaponRecharges>& saved) {
        recharges_ = saved;
    }
    bool LoadEffects(const Character& saved, unsigned profession) {
        modern::ConditionState<14505> conditions;
        const auto negative = saved.profileFields.find("condition_effects");
        if (negative != saved.profileFields.end() && !conditions.Load(negative->second))
            return false;
        const auto at = saved.profileFields.find("initiative_state");
        if (at != saved.profileFields.end() && !at->second.empty()) {
            auto resource = modern::InitiativeLedger::ForBuild(14812);
            if (profession != 5 || !resource.Load(at->second))
                return false;
        }
        return stance_.Load(saved, profession);
    }
    void SetProfession(uint8_t profession, uint8_t level, uint8_t state) {
        profession_ = profession;
        level_ = level;
        attunement_ = state;
    }
    void SetWeaponEquipped(bool equipped) override {
        weaponEquipped_ = equipped;
        if (!equipped) {
            symbol_.Cancel();
            if (skill_.plan.count && skill_.plan.skill == beta::Protection965::Skill)
                protectionCancelled_ = true;
            if (skill_.plan.count && skill_.plan.skill == beta::RockBarrier6224::Skill)
                rocksCancelled_ = true;
        }
    }
    void SetAquaticWeaponEquipped(bool equipped) override {
        aquaticWeaponEquipped_ = equipped;
    }
    uint64_t PeerEffectVersion() const override {
        return effectVersion_;
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        for (const auto& shot : shots_)
            if (shot.flight.phase == 2) {
                Buffer bytes;
                if (!DestroyAgent(shot.flight.Agent(agent_), sequence, bytes) || !out.Append(bytes.View()))
                    return false;
            }
        if (projectile_.phase != 2)
            return true;
        Buffer bytes;
        return DestroyAgent(MissileAgent(agent_), sequence, bytes) && out.Append(bytes.View());
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        if (context.peerAgent != agent_ || !context.sequence)
            return WorldActionResult::Failed;
        Buffer result;
        if (!seen && skill_.plan.count && skill_.next) {
            const auto& stage = skill_.plan.stages[skill_.next - 1];
            const auto remaining = static_cast<uint16_t>(
                (std::min)(skill_.at > context.now ? skill_.at - context.now : 1, uint64_t(stage.durationMs)));
            const std::array<float, 3> position{{context.position.x, context.position.y, context.position.z}};
            if (!SkillStageReply(agent_, context.sequence, skill_.identity, skill_.plan.skill, skill_.plan.slot,
                                 skill_.target, stage.index, true, skill_.next == skill_.plan.count, remaining,
                                 position, result, skill_.plan.state,
                                 skill_.emittedMoving ? &skill_.emittedMovement : nullptr, skill_.plan.underwater))
                return WorldActionResult::Failed;
        }
        if (mediumReported_ && !WeaponSelection(result))
            return WorldActionResult::Failed;
        if (attunementReported_ && !ProfessionSelection(result))
            return WorldActionResult::Failed;
        if (!stance_.Snapshot(agent_, context.now, level_, result))
            return WorldActionResult::Failed;
        if (!seen && !conditions_.Snapshot(agent_, context.wallNow, level_, result))
            return WorldActionResult::Failed;
        for (const auto& shot : shots_) {
            Buffer bytes;
            if (shot.flight.phase == 2 && context.now < shot.flight.at) {
                if (!shot.flight.Snapshot(14812, agent_, context.now, context.sequence, bytes))
                    return WorldActionResult::Failed;
            } else if (seen && shot.flight.skill) {
                if (!DestroyAgent(shot.flight.Agent(agent_), context.sequence, bytes))
                    return WorldActionResult::Failed;
            }
            if (!result.Append(bytes.View()))
                return WorldActionResult::Failed;
        }
        if (projectile_.phase != 2 || context.now >= projectile_.at) {
            if (seen && projectile_.skill) {
                Buffer destroy;
                if (!DestroyAgent(MissileAgent(agent_), context.sequence, destroy) || !result.Append(destroy.View()))
                    return WorldActionResult::Failed;
            }
            if (!result.Size())
                return WorldActionResult::Ignored;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        Buffer create;
        if (!projectile_.Snapshot(14812, agent_, context.now, context.sequence, create))
            return WorldActionResult::Failed;
        if (!result.Append(create.View()))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        if (healthRecoveryRequired_)
            return WorldActionResult::Failed;
        MotionMessage m;
        size_t consumed = 0;
        const auto status = DecodeMotion(input, m, consumed);
        if (status == DecodeStatus::Unknown)
            return WorldActionResult::Ignored;
        if (status != DecodeStatus::Complete || consumed != input.size || !ValidContext(context) ||
            !ValidOwnedMotion(input, agent_))
            return WorldActionResult::Failed;
        uint32_t dodgeToken = 0;
        if (m.id == 8 && m.hasOptional &&
            beta::DodgeCommand(14812, m.state, m.control, {m.input, m.inputSize}, dodgeToken))
            return Dodge(input, dodgeToken, context, out);
        if (stance_.RockDeadline() && context.wallNow >= stance_.RockDeadline())
            return Tick(context, out);
        auto next = *this;
        WorldActionOutput result;
        Cursor cursor{context.sequence};
        // Original C13 report: collider state 3 is deep water, 4 surface,
        // 0/1/2 ground/air/jump. Unknown state 5 does not choose equipment.
        if (loadout_ && m.id == 13 && m.mode < 5) {
            const uint8_t medium = m.mode == 3 ? 1 : m.mode == 4 ? 2 : 0;
            next.mediumKnown_ = true;
            if (medium != next.medium_) {
                if (next.skill_.plan.count && !next.CancelSkill(context, cursor, result))
                    return WorldActionResult::Failed;
                for (auto& shot : next.shots_) {
                    if (shot.flight.phase == 2) {
                        uint32_t sequence;
                        Buffer bytes;
                        if (!cursor.Next(sequence) || !DestroyAgent(shot.flight.Agent(agent_), sequence, bytes) ||
                            !result.owned.Append(bytes.View()) || !result.peers.Append(bytes.View()))
                            return WorldActionResult::Failed;
                    }
                    shot.flight.phase = 0;
                }
                if (next.projectile_.phase == 2) {
                    uint32_t sequence;
                    Buffer destroy;
                    if (!cursor.Next(sequence) || !DestroyAgent(MissileAgent(agent_), sequence, destroy) ||
                        !result.owned.Append(destroy.View()) || !result.peers.Append(destroy.View()))
                        return WorldActionResult::Failed;
                }
                next.projectile_.phase = 0;
                next.phoenix_ = {};
                if (next.effectVersion_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                next.medium_ = medium;
                next.mediumReported_ = true;
                next.symbol_.Cancel();
                next.previous_.fill(0);
                ++next.effectVersion_;
                Buffer selection;
                if (!next.WeaponSelection(selection) || !result.owned.Append(selection.View()) ||
                    !result.peers.Append(selection.View()))
                    return WorldActionResult::Failed;
            }
        }
        if (!next.Advance(context, cursor, result))
            return WorldActionResult::Failed;
        SkillPlan plan;
        uint32_t target = 0;
        bool ground = false;
        const uint32_t identity = m.control & 0x7fffffffu;
        // Native F1..F4 use directed skill slots12..15. These own skills are
        // instant and have no animation; S216 refreshes both weapon palettes.
        if (loadout_ && profession_ == 6 && m.id == 15 && (m.control & 0x80000000u) && identity &&
            identity != next.lastIdentity_ && m.inputSize == 6 && m.input[0] == 0x36 && m.input[5] >= 12 &&
            m.input[5] <= 15) {
            const uint8_t state = static_cast<uint8_t>(m.input[5] - 11);
            if ((state < 3 || level_ >= 3) && (state < 4 || level_ >= 6)) {
                next.lastIdentity_ = identity;
                if (state != next.attunement_) {
                    if (next.skill_.plan.count && !next.CancelSkill(context, cursor, result))
                        return WorldActionResult::Failed;
                    // A cancelled cast cannot launch later. Already airborne
                    // projectiles keep their independent, accepted lifetime.
                    for (auto& shot : next.shots_)
                        if (shot.flight.phase == 1)
                            shot.flight.phase = 0;
                    if (next.projectile_.phase == 1)
                        next.projectile_.phase = 0;
                    if (next.effectVersion_ == UINT64_MAX)
                        return WorldActionResult::Failed;
                    next.attunement_ = state;
                    next.attunementReported_ = true;
                    next.previous_.fill(0);
                    ++next.effectVersion_;
                    Buffer selection;
                    if (!next.ProfessionSelection(selection) || !result.owned.Append(selection.View()) ||
                        !result.peers.Append(selection.View()))
                        return WorldActionResult::Failed;
                }
            }
        }
        if (pilot_ && (next.medium_ == 1 ? aquaticWeaponEquipped_ : weaponEquipped_) && m.id == 15 &&
            (m.control & 0x80000000u) && identity &&
            (catalog_.Candidates().empty() || identity != next.lastIdentity_)) {
            if (m.inputSize == 6 && m.input[0] == 0x36) {
                std::memcpy(&target, m.input + 1, 4);
                if (!target || target == agent_ ||
                    (context.isVisibleCharacterTarget && context.isVisibleCharacterTarget(target)))
                    next.DirectedPlan(m.input[5], context.now, plan);
            } else if (!next.medium_ && m.inputSize == 18 && m.input[0] == 0x37 && !next.projectile_.phase) {
                ground = next.GroundPlan(m, context, plan);
            }
        }
        const bool consumeStance = plan.skill == 11282 && next.stance_.ActiveBuff(context.now) == 8964;
        if ((plan.instant && identity == next.lastInstantIdentity_) ||
            (!plan.instant && next.skill_.plan.count && !(consumeStance && next.skill_.plan.skill == 8962)))
            plan = {};
        // This personal pair has authored range0. An incidental friendly
        // selection must not redirect it or suppress it through target range.
        if (profession_ == 5 && loadout_ == 6 && next.medium_ == 1 && (plan.skill == 8962 || consumeStance))
            target = 0;
        if (plan.skill == beta::PersonalSwiftness4284::Skill || plan.skill == beta::Protection965::Skill ||
            plan.skill == beta::RockBarrier6224::Skill)
            target = 0;
        // AnnouncePeer creates the actor with yaw zero. Use that known initial
        // facing until an actual direction/rotation report replaces it.
        std::array<float, 3> facing = context.hasFacing
                                          ? std::array<float, 3>{{context.facing.x, context.facing.y, context.facing.z}}
                                          : std::array<float, 3>{{1, 0, 0}};
        if (plan.skill && target && plan.hasRange) {
            CatalogPosition point;
            double distance = 0;
            if (!next.TargetPosition(target, context, point, distance) || !next.InRange(plan, distance))
                plan = {};
            else if (distance > 0.0001)
                facing = {{point.x - context.position.x, point.y - context.position.y,
                           plan.underwater ? context.position.z - point.z : 0}};
        }
        for (unsigned i = 0; i < plan.count; ++i)
            if (plan.stages[i].type) {
                SkillMovement move;
                if (target == agent_ || !BuildSkillMovement(plan.stages[i], facing, move, plan.underwater)) {
                    plan = {};
                    break;
                }
            }
        if (plan.skill && FindWeaponProjectileRule(plan.skill) && target == agent_)
            plan = {};
        if (plan.skill && (!next.Admit(plan) || (FindWeaponProjectileRule(plan.skill) && !next.ShotAvailable()))) {
            // Existing cooling-skill policy suppresses the echo and all cast mutation.
            return Commit(next, context, cursor, result, out, false, true);
        }
        if (plan.skill == beta::RockBarrier6224::Hurl) {
            Buffer effect;
            if (next.effectVersion_ == UINT64_MAX)
                return WorldActionResult::Failed;
            const int prepared =
                beta::PrepareRockThrow(agent_, identity, target, context, next.shots_, next.stance_, effect);
            for (auto& shot : next.shots_)
                if (shot.flight.phase == 1 && shot.flight.identity == identity)
                    shot.contact.Capture(14812, shot.flight.skill, target, context);
            if (prepared < 0)
                return WorldActionResult::Failed;
            if (!prepared)
                return Commit(next, context, cursor, result, out, false, true);
            if (!result.owned.Append(effect.View()) || !result.peers.Append(effect.View()))
                return WorldActionResult::Failed;
            if (!next.stance_.RockCount(context.now) && !next.RockRecharge(context.wallNow, context, result))
                return WorldActionResult::Failed;
            result.peerEffectVersion = ++next.effectVersion_;
        }
        if (m.id != 12 && m.id != 13) {
            uint32_t sequence;
            Buffer echo;
            if (!cursor.Next(sequence) || !EncodeMotion(m, agent_, sequence, echo) ||
                !result.peers.Append(echo.View()) || !result.owned.Append(echo.View()))
                return WorldActionResult::Failed;
        }
        if (m.id == 12)
            next.scheduleDelay_ = m.scheduleDelay;
        if (plan.skill) {
            if (plan.skill == beta::Protection965::Skill) {
                next.protectionCancelled_ = false;
                next.counterTarget_ = 0;
                next.counterIdentity_.clear();
            }
            if (plan.skill == beta::RockBarrier6224::Skill)
                next.rocksCancelled_ = false;
            next.symbol_.CancelPending();
            if (plan.skill == beta::Symbol2527::Skill && !next.symbol_.Schedule(context))
                return WorldActionResult::Failed;
            if (plan.skill == beta::PersonalSwiftness4284::Skill) {
                Buffer effect;
                if (next.effectVersion_ == UINT64_MAX ||
                    !next.stance_.StartPersonal(agent_, plan.skill, context, level_, effect) ||
                    !result.owned.Append(effect.View()) || !result.peers.Append(effect.View()))
                    return WorldActionResult::Failed;
                result.peerEffectVersion = ++next.effectVersion_;
            }
            if (consumeStance && next.skill_.plan.count && !next.CancelSkill(context, cursor, result))
                return WorldActionResult::Failed;
            const bool startStance = profession_ == 5 && loadout_ == 6 && next.medium_ == 1 && plan.skill == 8962;
            if (startStance || consumeStance) {
                Buffer effect;
                if (next.effectVersion_ == UINT64_MAX)
                    return WorldActionResult::Failed;
                if (consumeStance) {
                    if (!next.stance_.Consume(agent_, effect))
                        return WorldActionResult::Failed;
                } else if (!next.stance_.Start(agent_, plan.skill, context.now, context.wallNow, level_, effect))
                    return WorldActionResult::Failed;
                if (!result.owned.Append(effect.View()) || !result.peers.Append(effect.View()))
                    return WorldActionResult::Failed;
                result.peerEffectVersion = ++next.effectVersion_;
            }
            if (plan.rechargeMs && plan.skill != beta::RockBarrier6224::Skill) {
                uint32_t duration = plan.rechargeMs;
                for (unsigned i = 0; i < plan.count; ++i)
                    duration += plan.stages[i].durationMs;
                uint64_t ready, utc;
                if (!context.wallNow || !Deadline(context.now, duration, ready) ||
                    !Deadline(context.wallNow, duration, utc))
                    return WorldActionResult::Failed;
                for (auto& entry : next.recharges_)
                    if (!entry.skill) {
                        entry = {plan.skill, ready, utc};
                        break;
                    }
            }
            if (plan.instant) {
                // July does not create a predicted order when Animation is
                // absent. Acknowledge and recharge without a fabricated phase.
                next.lastInstantIdentity_ = identity;
                if (!next.BeginRecharge(plan, context, result))
                    return WorldActionResult::Failed;
            } else {
                next.lastIdentity_ = identity;
                next.skill_ = {};
                next.skill_.plan = plan;
                next.skill_.identity = identity;
                next.skill_.target = target;
                next.skill_.facing = facing;
                next.melee_ = {};
                if (plan.skill != 965 && beta::MeleeCombatSkill(14812, plan.skill) && plan.hasRange && target &&
                    context.combatTarget) {
                    WorldCombatTarget enemy;
                    if (context.combatTarget(target, enemy) && enemy.actor.agent == target &&
                        !enemy.actor.identity.empty()) {
                        next.melee_.skill = plan.skill;
                        next.melee_.identity = identity;
                        next.melee_.target = target;
                        next.melee_.targetIdentity = enemy.actor.identity;
                        next.melee_.minimum = plan.rangeMin;
                        next.melee_.maximum = plan.rangeMax;
                        next.melee_.medium = next.medium_;
                    }
                }
                if (!next.effectVersion_)
                    next.effectVersion_ = 1;
                result.peerEffectVersion = next.effectVersion_;
            }
            const bool waterFlight = ground && beta::JulyWaterGroundFlight::Supports(plan.skill);
            if (ground && (plan.skill == 5258 || waterFlight)) {
                next.phoenix_ = {};
                next.projectile_ = {};
                auto& projectile = next.projectile_;
                projectile.phase = 1;
                projectile.identity = identity;
                projectile.skill = plan.skill;
                projectile.mode = waterFlight ? beta::JulyWaterGroundFlight::Mode : 1;
                projectile.speed = waterFlight ? static_cast<float>(beta::JulyWaterGroundFlight::Speed) : 600.0f;
                std::memcpy(projectile.destination.data(), m.input + 1, 12);
                std::memcpy(&projectile.token, m.input + 13, 4);
                if (!Deadline(context.now, waterFlight ? beta::JulyWaterGroundFlight::LaunchMs : 700, projectile.at))
                    return WorldActionResult::Failed;
            }
            if (const auto* rule = FindWeaponProjectileRule(plan.skill)) {
                if (!plan.hasRange || plan.rangeMax <= 1 || target == agent_)
                    return WorldActionResult::Failed;
                size_t index = 0;
                while (index < next.shots_.size() && next.shots_[index].flight.phase)
                    ++index;
                if (index == next.shots_.size())
                    return WorldActionResult::Failed;
                auto& shot = next.shots_[index];
                shot = {};
                shot.target = target;
                shot.range = plan.rangeMax;
                shot.facing = facing;
                shot.contact.Capture(14812, plan.skill, target, context);
                auto& flight = shot.flight;
                flight.phase = 1;
                flight.identity = identity;
                flight.skill = plan.skill;
                flight.agent = static_cast<uint16_t>(MissileAgent(agent_) + 256 * (index + 1));
                flight.mode = 2;
                flight.speed = rule->speed;
                if (!Deadline(context.now, rule->launchMs, flight.at))
                    return WorldActionResult::Failed;
            }
            if (!plan.instant && !next.AdvanceSkill(context, cursor, result))
                return WorldActionResult::Failed;
        }
        if (next.attunement_ != attunement_) {
            if (!context.ReserveSequences(cursor.used))
                return WorldActionResult::Failed;
            if (!context.persistProfessionState || !context.persistProfessionState(next.attunement_)) {
                WorldActionOutput rejected;
                rejected.sequences = 0;
                out = std::move(rejected);
                return WorldActionResult::Handled;
            }
        }
        return Commit(next, context, cursor, result, out,
                      plan.skill && plan.rechargeMs && plan.skill != beta::RockBarrier6224::Skill, true, plan.skill);
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& c, const ReceivedWorldEffect& request,
                                            std::unique_ptr<PreparedWorldEffect>& out) override {
        return PrepareReceivedEffects(c, {request}, out);
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& c,
                                             const std::vector<ReceivedWorldEffect>& requests,
                                             std::unique_ptr<PreparedWorldEffect>& out) override {
        if (healthRecoveryRequired_)
            return WorldActionResult::Failed;
        return beta::PreparePositiveEffects(stance_, effectVersion_, agent_, level_, c, requests, out, true);
    }
    bool EvadesIncoming(uint64_t now) const override {
        return dodge_.Active(now);
    }
    bool BlocksIncoming(uint64_t now) const override {
        if (protectionCancelled_ || counterTarget_ || medium_ != 0 || !weaponEquipped_ || skill_.plan.skill != 965 ||
            !skill_.plan.count || !skill_.next || skill_.next > skill_.plan.count)
            return false;
        const auto phase = skill_.plan.stages[skill_.next - 1].index;
        if (phase < 1 || phase >= 4)
            return false;
        uint64_t end = skill_.at;
        for (unsigned i = skill_.next; i < skill_.plan.count && skill_.plan.stages[i].index < 4; ++i) {
            if (end > UINT64_MAX - skill_.plan.stages[i].durationMs)
                return false;
            end += skill_.plan.stages[i].durationMs;
        }
        return now < end;
    }
    bool BlockIncoming(uint16_t source, const std::string& identity, uint64_t now) override {
        if (!source || identity.empty() || !BlocksIncoming(now))
            return false;
        counterTarget_ = source;
        counterIdentity_ = identity;
        return true;
    }
    bool CounterPending() const {
        return counterTarget_ && !protectionCancelled_ && medium_ == 0 && weaponEquipped_ && skill_.plan.skill == 965 &&
               skill_.plan.count;
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        if (CounterPending())
            return true;
        bool projectile = false;
        for (const auto& shot : shots_)
            if (shot.flight.phase == 2 && shot.contact.skill &&
                now > (shot.contact.sampled ? shot.contact.sampledAt : shot.flight.launchedAt))
                projectile = true;
        WorldActionContext c;
        c.now = now;
        if (!projectile && !MeleeOutcomeDue(c))
            return false;
        MotionMessage m;
        size_t consumed = 0;
        if (DecodeMotion(input, m, consumed) != DecodeStatus::Complete || consumed != input.size)
            return false;
        // A medium change cancels the old action instead of landing its hit.
        return !(m.id == 13 && m.mode < 5 && (m.mode == 3 ? 1 : m.mode == 4 ? 2 : 0) != medium_);
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (context.wallNow && !dodge_.Advance(context.wallNow, context.skillEffects))
            return WorldActionResult::Failed;
        if (healthRecoveryRequired_ || !ValidContext(context))
            return WorldActionResult::Failed;
        if (MeleeOutcomeDue(context))
            return FinishMelee(context, out);
        if (CounterPending())
            return StartCounter(context, out);
        const int synced =
            beta::SyncConditions(conditions_, level_, effectVersion_, RechargeText(), stance_, context, out);
        if (synced < 0)
            return WorldActionResult::Failed;
        if (!synced)
            return WorldActionResult::Handled;
        if (synced == 2) {
            lastNow_ = context.now;
            return WorldActionResult::Handled;
        }
        const bool protection = ProtectionOutcomeDue(context), rocks = RockOutcomeDue(context);
        const bool phoenix = phoenix_.Due(projectile_, context);
        if (WaterHealingDue(context) || protection || rocks || phoenix) {
            if (stance_.HasRegeneration()) {
                auto positive = *this;
                WorldActionOutput result;
                Cursor cursor{context.sequence};
                if (!positive.Restore(context, result) || !positive.AdvanceStance(context, result))
                    return WorldActionResult::Failed;
                if (positive.stance_.Text() != stance_.Text() || result.owned.Size())
                    return Commit(positive, context, cursor, result, out, false, false);
            }
            if (phoenix) {
                auto next = *this;
                const int returned =
                    beta::AdvancePhoenix(14812, agent_, level_, next.projectile_, next.phoenix_, next.stance_,
                                         next.conditions_, next.effectVersion_, next.RechargeText(), context, out);
                if (returned < 0)
                    return WorldActionResult::Failed;
                if (!returned)
                    return WorldActionResult::Handled;
                if (returned == 2) {
                    next.lastNow_ = context.now;
                    *this = std::move(next);
                    return WorldActionResult::Handled;
                }
            } else
                return rocks        ? FinishRocks(context, out)
                       : protection ? FinishProtection(context, out)
                                    : FinishWaterHealing(context, out);
        }
        auto next = *this;
        WorldActionOutput result;
        Cursor cursor{context.sequence};
        const int judgment = beta::AdvanceJudgmentHealing(14812, agent_, level_, next.shots_, next.effectVersion_,
                                                          next.healthRecoveryRequired_, context, result);
        if (judgment < 0) {
            if (next.healthRecoveryRequired_)
                healthRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (!judgment) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        if (judgment == 2) {
            next.lastNow_ = context.now;
            *this = std::move(next);
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        const int siren = beta::AdvanceSirenContact(agent_, next.shots_, next.effectVersion_,
                                                    next.healthRecoveryRequired_, next.RechargeText(), context, result);
        if (siren < 0) {
            if (next.healthRecoveryRequired_)
                healthRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (!siren) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        if (siren == 2) {
            next.lastNow_ = context.now;
            *this = std::move(next);
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        const int projectileHit = beta::AdvanceProjectileCombat(14812, agent_, next.shots_, next.effectVersion_,
                                                                next.healthRecoveryRequired_, context, result);
        if (projectileHit < 0) {
            if (next.healthRecoveryRequired_)
                healthRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (projectileHit == 2) {
            next.lastNow_ = context.now;
            *this = std::move(next);
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        beta::KeepProjectileSamples(shots_, next.shots_);
        if (!projectileHit) {
            out = {};
            out.sequences = 0;
            return WorldActionResult::Handled;
        }
        if (!next.Advance(context, cursor, result))
            return WorldActionResult::Failed;
        return Commit(next, context, cursor, result, out, false, false);
    }

  private:
    bool RockRecharge(uint64_t start, const WorldActionContext& c, WorldActionOutput& out) {
        using R = beta::RockBarrier6224;
        if (!start || !c.wallNow || start > UINT64_MAX - R::RechargeMs)
            return false;
        const auto end = start + R::RechargeMs, remaining = end > c.wallNow ? end - c.wallNow : 0;
        if (c.now > UINT64_MAX - remaining)
            return false;
        if (remaining) {
            auto entry = std::find_if(recharges_.begin(), recharges_.end(),
                                      [](const Recharge& r) { return r.skill == R::Skill; });
            if (entry == recharges_.end())
                entry = std::find_if(recharges_.begin(), recharges_.end(), [](const Recharge& r) { return !r.skill; });
            if (entry == recharges_.end())
                return false;
            *entry = {R::Skill, c.now + remaining, end};
        } else
            for (auto& entry : recharges_)
                if (entry.skill == R::Skill)
                    entry = {};
        Buffer bytes;
        return SkillRecharge(agent_, R::Skill, static_cast<uint32_t>((std::min)(remaining, uint64_t(R::RechargeMs))),
                             R::RechargeMs, bytes) &&
               out.owned.Append(bytes.View());
    }
    bool MeleeOutcomeDue(const WorldActionContext& c) const {
        return melee_.skill &&
               (!skill_.plan.count || skill_.plan.skill != melee_.skill || skill_.identity != melee_.identity ||
                medium_ != melee_.medium || !(medium_ == 1 ? aquaticWeaponEquipped_ : weaponEquipped_) ||
                (skill_.next >= skill_.plan.count && c.now >= skill_.at));
    }
    WorldActionResult FinishMelee(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = *this;
        if (!skill_.plan.count || skill_.plan.skill != melee_.skill || skill_.identity != melee_.identity ||
            medium_ != melee_.medium || !(medium_ == 1 ? aquaticWeaponEquipped_ : weaponEquipped_)) {
            next.melee_ = {};
            out = {};
            out.sequences = 0;
        } else {
            const int hit = next.melee_.Settle(14812, agent_, level_, c, out);
            if (hit < 0) {
                healthRecoveryRequired_ = true;
                return WorldActionResult::Failed;
            }
            if (!hit)
                return WorldActionResult::Handled;
        }
        next.lastNow_ = c.now;
        *this = std::move(next);
        return WorldActionResult::Handled;
    }
    bool RockOutcomeDue(const WorldActionContext& c) const {
        return !rocksCancelled_ && medium_ == 0 && weaponEquipped_ &&
               skill_.plan.skill == beta::RockBarrier6224::Skill && skill_.plan.count &&
               skill_.next >= skill_.plan.count && c.now >= skill_.at;
    }
    WorldActionResult FinishRocks(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = *this;
        WorldActionOutput result;
        Cursor cursor{c.sequence};
        Buffer effect;
        next.healthAfter_ = c.healthState;
        if (next.effectVersion_ == UINT64_MAX || !next.AdvanceSkill(c, cursor, result) || cursor.used ||
            !next.stance_.StartRocks(agent_, c, level_, effect) || !result.owned.Append(effect.View()) ||
            !result.peers.Append(effect.View()))
            return WorldActionResult::Failed;
        result.peerEffectVersion = ++next.effectVersion_;
        return Commit(next, c, cursor, result, out, false, false);
    }
    bool ProtectionOutcomeDue(const WorldActionContext& c) const {
        return c.persistPreparedAlliedEffects && !protectionCancelled_ && medium_ == 0 && weaponEquipped_ &&
               skill_.plan.skill == beta::Protection965::Skill && skill_.plan.count &&
               skill_.next < skill_.plan.count &&
               skill_.plan.stages[skill_.next].index == beta::Protection965::OutcomePhase && c.now >= skill_.at;
    }
    WorldActionResult Dodge(ByteView input, uint32_t token, const WorldActionContext& c, WorldActionOutput& out) {
        out = {};
        out.sequences = 0;
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health, beta::BetaHealthMaximum(14812)))
            return WorldActionResult::Failed;
        auto next = *this;
        if (health <= 0 || !next.dodge_.Accept(token, c.now, c.wallNow, c.skillEffects))
            return WorldActionResult::Handled;
        WorldActionOutput result;
        Cursor cursor{c.sequence};
        if (next.skill_.plan.count && !next.CancelSkill(c, cursor, result))
            return WorldActionResult::Failed;
        uint32_t sequence = 0;
        Buffer own, peer;
        if (!cursor.Next(sequence) || OwnMotion(input, agent_, sequence, own) != RelayResult::Encoded ||
            OwnMotion(input, c.peerAgent, sequence, peer) != RelayResult::Encoded || !result.owned.Append(own.View()) ||
            !result.peers.Append(peer.View()) || !c.ReserveSequences(cursor.used))
            return WorldActionResult::Failed;
        next.melee_ = {};
        next.symbol_.CancelPending();
        next.protectionCancelled_ = next.rocksCancelled_ = true;
        next.counterTarget_ = 0;
        next.counterIdentity_.clear();
        for (auto& shot : next.shots_)
            if (shot.flight.phase == 1) {
                shot.flight.phase = 0;
                shot.contact = {};
                shot.healing = {};
            }
        if (next.projectile_.phase == 1) {
            next.projectile_.phase = 0;
            next.phoenix_ = {};
        }
        result.sequences = cursor.used;
        next.lastNow_ = c.now;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult StartCounter(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = *this;
        unsigned finish = next.skill_.next;
        while (finish < next.skill_.plan.count && next.skill_.plan.stages[finish].index != 4)
            ++finish;
        if (finish == next.skill_.plan.count || !next.skill_.next)
            return WorldActionResult::Failed;
        unsigned closing = finish;
        if (next.skill_.plan.stages[next.skill_.next - 1].index != 3)
            for (unsigned i = next.skill_.next; i < finish; ++i)
                if (next.skill_.plan.stages[i].index == 3) {
                    closing = i;
                    break;
                }
        unsigned written = next.skill_.next;
        if (closing < finish)
            next.skill_.plan.stages[written++] = next.skill_.plan.stages[closing];
        for (unsigned i = finish; i < next.skill_.plan.count; ++i)
            next.skill_.plan.stages[written++] = next.skill_.plan.stages[i];
        next.skill_.plan.count = static_cast<uint8_t>(written);
        next.skill_.at = c.now;
        next.skill_.target = 0;
        next.melee_ = {};
        next.melee_.skill = 965;
        next.melee_.identity = next.skill_.identity;
        next.melee_.maximum = 170;
        next.melee_.medium = next.medium_;
        next.protectionCancelled_ = true;
        next.counterTarget_ = 0;
        next.counterIdentity_.clear();
        WorldActionOutput result;
        Cursor cursor{c.sequence};
        if (!next.AdvanceSkill(c, cursor, result) || cursor.used != 1 || !c.ReserveSequences(cursor.used))
            return WorldActionResult::Failed;
        result.sequences = cursor.used;
        next.lastNow_ = c.now;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult FinishProtection(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = *this;
        WorldActionOutput result;
        Cursor cursor{c.sequence};
        if (!next.AdvanceSkill(c, cursor, result) || cursor.used != 1)
            return WorldActionResult::Failed;
        result.sequences = cursor.used;
        beta::Protection965Execution outcome;
        if (!outcome.Schedule(c))
            return WorldActionResult::Failed;
        const int applied = outcome.Advance(14812, agent_, level_, next.stance_, next.effectVersion_,
                                            next.healthRecoveryRequired_, next.RechargeText(), c, result);
        if (applied < 0) {
            if (next.healthRecoveryRequired_)
                healthRecoveryRequired_ = true;
            return WorldActionResult::Failed;
        }
        if (applied != 2) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        next.lastNow_ = c.now;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool WaterHealingDue(const WorldActionContext& context) const {
        if (!projectile_.phase || projectile_.skill != 1155 || context.now < projectile_.at)
            return false;
        if (projectile_.phase == 2)
            return true;
        std::array<float, 3> start;
        if (!NativePosition(context.position.x, context.position.y, context.position.z, start))
            return false;
        double distance = 0;
        for (size_t i = 0; i < 3; ++i)
            distance += std::pow(double(start[i]) - projectile_.destination[i], 2);
        return distance <= 1;
    }
    WorldActionResult FinishWaterHealing(const WorldActionContext& context, WorldActionOutput& out) {
        beta::PreparedWaterHealing prepared;
        if (!context.initiativeState.empty() ||
            !beta::PrepareWaterHealing(14812, agent_, projectile_.destination, context, prepared))
            return WorldActionResult::Failed;
        auto next = *this;
        next.projectile_.phase = 0;
        WorldActionOutput result;
        Cursor cursor{context.sequence};
        // Preserve any due phase/recharge output on the disposable copy. All
        // encoding and validation precede the durable allied-health commit.
        if (!next.Advance(context, cursor, result) || next.effectVersion_ == UINT64_MAX ||
            !result.owned.Append(prepared.owned.View()) || !result.peers.Append(prepared.peers.View()))
            return WorldActionResult::Failed;
        if (projectile_.phase == 2) {
            for (uint16_t healed : prepared.healedAgents) {
                uint32_t sequence;
                Buffer hit;
                if (!cursor.Next(sequence) ||
                    !beta::BetaMissileHit(14812, projectile_.Agent(agent_), sequence, projectile_.identity, healed,
                                          hit) ||
                    !result.owned.Append(hit.View()) || !result.peers.Append(hit.View()))
                    return WorldActionResult::Failed;
            }
            uint32_t sequence;
            Buffer destroy;
            if (!cursor.Next(sequence) || !DestroyAgent(projectile_.Agent(agent_), sequence, destroy) ||
                !result.owned.Append(destroy.View()) || !result.peers.Append(destroy.View()))
                return WorldActionResult::Failed;
        }
        if (!context.ReserveSequences(cursor.used))
            return WorldActionResult::Failed;
        if (!prepared.healedAgents.empty()) {
            if (!context.persistAlliedHealth)
                return WorldActionResult::Failed;
            const auto committed = context.persistAlliedHealth(prepared.ownHealth, prepared.targets);
            if (committed == WorldConditionCommit::RecoveryRequired) {
                healthRecoveryRequired_ = true;
                return WorldActionResult::Failed;
            }
            if (committed != WorldConditionCommit::Applied) {
                WorldActionOutput retry;
                retry.sequences = 0;
                out = std::move(retry);
                return WorldActionResult::Handled;
            }
        }
        next.lastNow_ = context.now;
        result.sequences = cursor.used;
        result.peerEffectVersion = ++next.effectVersion_;
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    // Every phase, effect, identity and medium change is still on the copy here.
    WorldActionResult Commit(JulyActions& next, const WorldActionContext& context, const Cursor& cursor,
                             WorldActionOutput& result, WorldActionOutput& out, bool rechargeChanged, bool request,
                             unsigned initiativeSkill = 0) {
        const bool initiativeEnabled = profession_ == 5 && bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return WorldActionResult::Failed;
        auto initiative = modern::InitiativeLedger::ForBuild(14812);
        if (initiativeEnabled && !initiative.Load(context.initiativeState))
            return WorldActionResult::Failed;
        const unsigned previous = initiative.Points();
        if (initiativeEnabled && !initiative.Advance(context.wallNow))
            return WorldActionResult::Failed;
        if (initiativeEnabled && initiativeSkill) {
            unsigned cost = 0;
            if (!modern::BetaInitialInitiativeCost(14812, initiativeSkill, cost))
                return WorldActionResult::Failed;
            if (!initiative.Spend(cost)) {
                WorldActionOutput empty;
                empty.sequences = 0;
                out = std::move(empty);
                return WorldActionResult::Handled;
            }
        }
        const bool initiativeSave =
            initiativeEnabled && (initiativeSkill || !initiativeAnnounced_ || context.initiativeState.empty() ||
                                  initiative.Points() != previous);
        if (initiativeSave && !modern::InitiativeCurrent(14812, agent_, initiative, result.owned))
            return WorldActionResult::Failed;
        const bool effectChanged = next.stance_.Text() != stance_.Text();
        bool reserved = false;
        if (initiativeSave || effectChanged || rechargeChanged) {
            if (!context.ReserveSequences(cursor.used))
                return WorldActionResult::Failed;
            reserved = true;
            const bool saved =
                initiativeEnabled
                    ? context.persistCombatInitiative(next.RechargeText(), next.stance_.Text(), next.healthAfter_,
                                                      context.conditionState, initiative.Text())
                : effectChanged
                    ? (next.stance_.HasRegeneration()
                           ? (context.persistCombatState &&
                              context.persistCombatState(next.RechargeText(), next.stance_.Text(), next.healthAfter_))
                           : (context.persistSkillState &&
                              context.persistSkillState(next.RechargeText(), next.stance_.Text())))
                    : (context.persistSkillRecharges && context.persistSkillRecharges(next.RechargeText()));
            if (!saved) {
                WorldActionOutput retry;
                retry.sequences = 0;
                out = std::move(retry);
                return WorldActionResult::Handled;
            }
        }
        if (initiativeSave)
            next.initiativeAnnounced_ = true;
        next.lastNow_ = context.now;
        result.sequences = cursor.used;
        const bool emitted = result.owned.Size() || result.peers.Size();
        *this = std::move(next);
        const int symbol = symbol_.Advance(14812, agent_, level_, stance_, effectVersion_, healthRecoveryRequired_,
                                           RechargeText(), context, result, reserved);
        if (symbol < 0)
            return WorldActionResult::Failed;
        if (!request && !emitted && symbol != 2 && !reserved)
            return WorldActionResult::Ignored;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool AdvanceStance(const WorldActionContext& context, WorldActionOutput& out) {
        const auto rockDeadline = stance_.RockDeadline();
        Buffer effect;
        bool changed = false, clockChanged = false;
        if (!stance_.PrepareHealth(14812, context, healthAfter_, clockChanged))
            return false;
        if (stance_.ActiveStance(context.now) && ((mediumKnown_ && medium_ != 1) || !aquaticWeaponEquipped_)) {
            if (!stance_.Consume(agent_, effect))
                return false;
            changed = true;
        } else if (!stance_.Advance(agent_, context.now, level_, effect, changed))
            return false;
        if (rockDeadline && !stance_.RockDeadline() && !RockRecharge(rockDeadline, context, out))
            return false;
        changed = changed || healthAfter_ != context.healthState;
        if (changed && effectVersion_ == UINT64_MAX)
            return false;
        if (!out.owned.Append(effect.View()) || !out.peers.Append(effect.View()))
            return false;
        if (healthAfter_ != context.healthState) {
            float current = 0;
            Buffer bytes;
            if (!modern::ReadHealthState(healthAfter_, current, beta::BetaHealthMaximum(14812)) ||
                !beta::BetaHealthUpdate(14812, agent_, current, bytes) || !out.owned.Append(bytes.View()) ||
                !out.peers.Append(bytes.View()))
                return false;
        }
        if (changed)
            out.peerEffectVersion = ++effectVersion_;
        return true;
    }
    const char* ProfessionState() const {
        static const char* states[] = {"None", "ElementalAttunementFire", "ElementalAttunementWater",
                                       "ElementalAttunementAir", "ElementalAttunementEarth"};
        return states[attunement_];
    }
    bool ProfessionSelection(Buffer& out) const {
        return out.U16(216) && out.VarU32(agent_) && out.VarU32(attunement_);
    }
    bool WeaponSelection(Buffer& out) const {
        return out.U16(129) && out.VarU32(agent_) && out.U8(medium_ == 1 ? 0 : 4);
    }
    std::string RechargeText() const {
        std::string text;
        for (const auto& entry : recharges_)
            if (entry.skill) {
                if (!text.empty())
                    text += ',';
                text += std::to_string(entry.skill) + ':' + std::to_string(entry.utc);
            }
        return text;
    }
    bool Restore(const WorldActionContext& context, WorldActionOutput& out) {
        if (restored_)
            return true;
        for (auto& entry : recharges_)
            if (entry.skill) {
                if (!context.wallNow)
                    return false;
                const auto remaining = entry.utc > context.wallNow ? entry.utc - context.wallNow : 0;
                uint32_t casting = 0;
                const auto total = RechargeDuration(entry.skill, catalog_, loadout_, &casting);
                if (!total || remaining > uint64_t(total) + casting || context.now > UINT64_MAX - remaining)
                    return false;
                entry.ready = context.now + remaining;
                Buffer bytes;
                if (!SkillRecharge(agent_, entry.skill, static_cast<uint32_t>((std::min)(remaining, uint64_t(total))),
                                   total, bytes) ||
                    !out.owned.Append(bytes.View()))
                    return false;
                if (!remaining)
                    entry = {};
            }
        if (!stance_.Restore(context.now, context.wallNow))
            return false;
        restored_ = true;
        return true;
    }
    bool ValidContext(const WorldActionContext& context) const {
        return context.peerAgent == agent_ && context.sequence && context.now >= lastNow_ &&
               std::isfinite(context.position.x * 32) && std::isfinite(context.position.y * 32) &&
               std::isfinite(context.position.z * 32);
    }
    bool DirectedPlan(unsigned slot, uint64_t now, SkillPlan& out) const {
        SkillPlan plan;
        plan.slot = static_cast<uint8_t>(slot);
        if (profession_ == 6 && attunement_ == 4 && medium_ == 0 && slot == 6 && stance_.RockCount(now)) {
            plan.skill = beta::RockBarrier6224::Hurl;
            plan.instant = true;
            plan.hasRange = true;
            plan.rangeMax = beta::RockBarrier6224::Range;
            out = plan;
            return true;
        }
        if (!catalog_.Candidates().empty()) {
            const uint32_t previous = slot < previous_.size() && now < chainUntil_ ? previous_[slot] : 0;
            const auto* skill = catalog_.Select(loadout_, medium_ == 1 ? 24 : 29, static_cast<uint8_t>(slot),
                                                ProfessionState(), previous, previous ? 1 : 0);
            if (profession_ == 5 && loadout_ == 6 && medium_ == 1 && slot == 7 && stance_.ActiveBuff(now) == 8964)
                skill = &ThiefStanceFollowup();
            if (!skill || skill->skillFlags.find("GroundTargeted") != std::string::npos)
                return false;
            if (medium_ == 2 && skill->skillFlags.find("UsableWaterSurface") == std::string::npos)
                return false;
            std::vector<june14505::SkillAnimationPhase> phases;
            std::string error;
            if (!june14505::BuildSkillPlan(*skill, phases, error) || phases.size() > 32)
                return false;
            plan.instant = phases.empty() && skill->skillFlags.find("Instant") != std::string::npos;
            if (phases.empty() && !plan.instant)
                return false;
            plan.skill = skill->skill;
            plan.rechargeMs = skill->recharge;
            plan.underwater = skill->underwater;
            plan.hasRange = skill->hasRange;
            plan.rangeMin = skill->rangeMin;
            plan.rangeMax = skill->rangeMax;
            plan.state = skill->skillFlags.find("AllowPlayerControl") == std::string::npos ? 0x43 : 0x53;
            for (const auto& phase : phases) {
                auto& stage = plan.stages[plan.count++];
                stage.index = phase.index;
                stage.durationMs = static_cast<uint16_t>(phase.duration);
                stage.type = phase.type;
                stage.movement = phase.movement;
            }
            if (!ValidSkillPlan(plan, slot))
                return false;
            out = plan;
            return true;
        }
        if (slot == 5) {
            plan.skill = 1153;
            plan.count = 5;
            plan.stages[0] = {0, 667};
            plan.stages[1] = {1, 200};
            plan.stages[2] = {2, 100};
            plan.stages[3] = {3, 200};
            plan.stages[4] = {4, 500};
        } else if (slot == 6) {
            plan.skill = 6221;
            plan.count = 1;
            plan.stages[0] = {0, 2067};
            plan.rechargeMs = 6000;
        } else
            return false;
        if (!ValidSkillPlan(plan, slot))
            return false;
        out = plan;
        return true;
    }
    bool GroundPlan(const MotionMessage& m, const WorldActionContext& context, SkillPlan& out) const {
        uint32_t token;
        std::memcpy(&token, m.input + 13, 4);
        if (token)
            return false;
        SkillPlan plan;
        plan.slot = m.input[17];
        if (catalog_.Candidates().empty()) {
            if (plan.slot != 7)
                return false;
            plan.skill = 5258;
            plan.count = 1;
            plan.stages[0] = {0, 1500};
            plan.rechargeMs = 20000;
            plan.hasRange = true;
            plan.rangeMax = 900;
        } else {
            const auto* skill = catalog_.Select(loadout_, 29, plan.slot, ProfessionState(), 0, 0);
            if (!skill || skill->skillFlags.find("GroundTargeted") == std::string::npos)
                return false;
            std::vector<june14505::SkillAnimationPhase> phases;
            std::string error;
            if (!june14505::BuildSkillPlan(*skill, phases, error) || phases.empty() || phases.size() > 32)
                return false;
            plan.skill = skill->skill;
            plan.rechargeMs = skill->recharge;
            plan.hasRange = skill->hasRange;
            plan.rangeMin = skill->rangeMin;
            plan.rangeMax = skill->rangeMax;
            plan.state = skill->skillFlags.find("AllowPlayerControl") == std::string::npos ? 0x43 : 0x53;
            for (const auto& phase : phases) {
                auto& stage = plan.stages[plan.count++];
                stage.index = phase.index;
                stage.durationMs = static_cast<uint16_t>(phase.duration);
                stage.type = phase.type;
                stage.movement = phase.movement;
            }
        }
        std::array<float, 3> start, point;
        std::memcpy(point.data(), m.input + 1, 12);
        if (!NativePosition(context.position.x, context.position.y, context.position.z, start))
            return false;
        double distance = 0, horizontal = 0;
        for (size_t i = 0; i < 3; ++i) {
            if (!std::isfinite(point[i]))
                return false;
            const double delta = static_cast<double>(point[i]) - start[i];
            distance += delta * delta;
            if (i < 2)
                horizontal += delta * delta;
        }
        if (!plan.hasRange || !InRange(plan, std::sqrt(distance)))
            return false;
        // Water retains zero-distance casts; the shared flight emits nothing
        // when start and destination coincide. Fire keeps its accepted bounds.
        if (plan.skill == 5258 && (horizontal <= 1 || std::sqrt(horizontal) / 600 > 30))
            return false;
        if (!ValidSkillPlan(plan, plan.slot))
            return false;
        out = plan;
        return true;
    }
    bool Admit(const SkillPlan& plan) const {
        bool available = false;
        for (const auto& entry : recharges_) {
            if (entry.skill == plan.skill)
                return false;
            if (!entry.skill)
                available = true;
        }
        return !plan.rechargeMs || available;
    }
    bool BeginRecharge(const SkillPlan& plan, const WorldActionContext& context, WorldActionOutput& out) {
        if (!plan.rechargeMs || plan.skill == beta::RockBarrier6224::Skill)
            return true;
        for (auto& entry : recharges_)
            if (entry.skill == plan.skill) {
                const auto remaining = entry.ready > context.now ? entry.ready - context.now : 0;
                Buffer bytes;
                if (!SkillRecharge(agent_, plan.skill,
                                   static_cast<uint32_t>((std::min)(remaining, uint64_t(plan.rechargeMs))),
                                   plan.rechargeMs, bytes) ||
                    !out.owned.Append(bytes.View()))
                    return false;
                return true;
            }
        return false;
    }
    bool TargetPosition(uint32_t target, const WorldActionContext& context, CatalogPosition& point,
                        double& distance) const {
        if (target == agent_)
            point = context.position;
        else if (!context.characterTargetPosition || !context.characterTargetPosition(target, point))
            return false;
        if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z))
            return false;
        const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                     z = double(point.z) - context.position.z;
        distance = std::sqrt(x * x + y * y + z * z) * 32;
        return std::isfinite(distance);
    }
    bool InRange(const SkillPlan& plan, double distance) const {
        return !plan.hasRange || (distance >= plan.rangeMin && distance <= plan.rangeMax);
    }
    bool CancelSkill(const WorldActionContext& context, Cursor& cursor, WorldActionOutput& out) {
        MotionMessage finish;
        finish.id = 15;
        finish.state = 0x50;
        finish.control = skill_.identity;
        finish.inputSize = 1;
        finish.input[0] = 0x38;
        uint32_t sequence;
        Buffer bytes;
        if (!cursor.Next(sequence) || !EncodeMotion(finish, agent_, sequence, bytes) ||
            !out.owned.Append(bytes.View()) || !out.peers.Append(bytes.View()) ||
            !BeginRecharge(skill_.plan, context, out))
            return false;
        skill_.plan.count = 0;
        melee_ = {};
        previous_.fill(0);
        return true;
    }
    bool AdvanceSkill(const WorldActionContext& context, Cursor& cursor, WorldActionOutput& out) {
        if (!skill_.plan.count || context.now < skill_.at)
            return true;
        if (skill_.next >= skill_.plan.count) {
            if (!BeginRecharge(skill_.plan, context, out) || !Deadline(context.now, 6000, chainUntil_))
                return false;
            previous_.fill(0);
            previous_[skill_.plan.slot] = skill_.plan.skill;
            skill_.plan.count = 0;
            return true;
        }
        const auto& stage = skill_.plan.stages[skill_.next];
        uint32_t sequence;
        uint64_t at;
        Buffer bytes;
        const std::array<float, 3> position{{context.position.x, context.position.y, context.position.z}};
        SkillMovement move;
        bool moving = stage.type != 0;
        auto facing = skill_.facing;
        double targetDistance = 0;
        if (skill_.target && skill_.plan.hasRange) {
            CatalogPosition point;
            if (!TargetPosition(skill_.target, context, point, targetDistance) || !InRange(skill_.plan, targetDistance))
                return CancelSkill(context, cursor, out);
            const double dx = double(point.x) - context.position.x, dy = double(point.y) - context.position.y;
            const double dz = skill_.plan.underwater ? double(point.z) - context.position.z : 0;
            targetDistance = std::hypot(std::hypot(dx, dy), dz) * 32;
            if (targetDistance <= 0.001)
                moving = false;
            // Agent directions use native Z (down); reported positions use physics Z (up).
            else
                facing = {{static_cast<float>(dx), static_cast<float>(dy), static_cast<float>(-dz)}};
        }
        if (moving) {
            if (!BuildSkillMovement(stage, facing, move, skill_.plan.underwater))
                return false;
            if (skill_.target)
                move.speed = (std::min)(move.speed, static_cast<float>(targetDistance * 1000 / stage.durationMs));
        }
        if (!Deadline(context.now, stage.durationMs, at) || !cursor.Next(sequence) ||
            !SkillStageReply(agent_, sequence, skill_.identity, skill_.plan.skill, skill_.plan.slot, skill_.target,
                             stage.index, skill_.next == 0, skill_.next + 1 == skill_.plan.count, stage.durationMs,
                             position, bytes, skill_.plan.state, moving ? &move : nullptr, skill_.plan.underwater) ||
            !out.owned.Append(bytes.View()) || !out.peers.Append(bytes.View()))
            return false;
        skill_.emittedMovement = move;
        skill_.emittedMoving = moving;
        ++skill_.next;
        skill_.at = at;
        return true;
    }
    bool AdvanceProjectile(const WorldActionContext& context, Cursor& cursor, WorldActionOutput& out) {
        if (WaterHealingDue(context))
            return true; // Tick owns the atomic health commit.
        if (projectile_.phase == 2 && projectile_.skill == beta::Phoenix5258::Skill && context.persistCombatConditions)
            return true;
        if (!projectile_.phase || context.now < projectile_.at)
            return true;
        std::array<float, 3> start{};
        if (projectile_.phase == 1 &&
            !NativePosition(context.position.x, context.position.y, context.position.z, start))
            return false;
        uint32_t sequence;
        Buffer bytes;
        if (!cursor.Next(sequence) || !projectile_.Advance(14812, agent_, start, context.now, sequence, bytes))
            return false;
        if (!bytes.Size()) {
            --cursor.used;
            return true;
        }
        if (effectVersion_ == UINT64_MAX || !out.owned.Append(bytes.View()) || !out.peers.Append(bytes.View()))
            return false;
        out.peerEffectVersion = ++effectVersion_;
        return true;
    }
    bool ShotAvailable() const {
        for (const auto& shot : shots_)
            if (!shot.flight.phase)
                return true;
        return false;
    }
    bool AdvanceShots(const WorldActionContext& context, Cursor& cursor, WorldActionOutput& out) {
        for (auto& shot : shots_) {
            auto& flight = shot.flight;
            if (!flight.phase || context.now < flight.at)
                continue;
            if (flight.phase == 2 && shot.contact.skill && context.now >= flight.at)
                continue;
            std::array<float, 3> start{};
            if (flight.phase == 1) {
                if (!shot.contact.Matches(context)) {
                    shot.flight.phase = 0;
                    shot.contact = {};
                    continue;
                }
                shot.contactLift = medium_ == 0 ? 39.0f : 0.0f;
                if (!NativePosition(context.position.x, context.position.y, context.position.z, start))
                    return false;
                if (shot.target) {
                    CatalogPosition point;
                    double distance = 0;
                    if (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(shot.target) ||
                        !TargetPosition(shot.target, context, point, distance) || distance > shot.range ||
                        distance <= 1) {
                        flight.phase = 0;
                        continue;
                    }
                    if (!NativePosition(point.x, point.y, point.z, flight.destination))
                        return false;
                } else {
                    const double length =
                        std::hypot(std::hypot(double(shot.facing[0]), double(shot.facing[1])), double(shot.facing[2]));
                    if (!std::isfinite(length) || length <= 0)
                        return false;
                    for (size_t i = 0; i < 3; ++i)
                        flight.destination[i] = start[i] + static_cast<float>(shot.facing[i] / length * shot.range);
                }
                if (medium_ == 0)
                    RaiseWeaponMissile(start, flight.destination);
            }
            uint32_t sequence;
            Buffer bytes;
            if (!cursor.Next(sequence) || !flight.Advance(14812, agent_, start, context.now, sequence, bytes))
                return false;
            if (!bytes.Size()) {
                --cursor.used;
                continue;
            }
            if (effectVersion_ == UINT64_MAX || !out.owned.Append(bytes.View()) || !out.peers.Append(bytes.View()))
                return false;
            out.peerEffectVersion = ++effectVersion_;
        }
        return true;
    }
    bool Advance(const WorldActionContext& context, Cursor& cursor, WorldActionOutput& out) {
        if (!Restore(context, out) || !AdvanceStance(context, out) || !AdvanceSkill(context, cursor, out) ||
            !AdvanceProjectile(context, cursor, out) || !AdvanceShots(context, cursor, out))
            return false;
        for (auto& entry : recharges_)
            if (entry.skill && context.now >= entry.ready) {
                Buffer bytes;
                if (!ClearSkillRecharge(agent_, entry.skill, bytes) || !out.owned.Append(bytes.View()))
                    return false;
                entry = {};
            }
        return true;
    }
    uint16_t agent_, scheduleDelay_ = 0;
    bool pilot_, weaponEquipped_ = false, aquaticWeaponEquipped_ = false, mediumReported_ = false;
    uint8_t medium_ = 0;
    uint64_t lastNow_ = 0;
    june14505::SkillCatalog catalog_{14812};
    uint32_t loadout_ = 0, lastIdentity_ = 0, lastInstantIdentity_ = 0;
    uint8_t profession_ = 0, level_ = 1, attunement_ = 1;
    bool attunementReported_ = false;
    bool initiativeAnnounced_ = false, healthRecoveryRequired_ = false;
    std::array<uint32_t, 17> previous_{};
    uint64_t chainUntil_ = 0;
    ActiveSkill skill_;
    Projectile projectile_;
    std::array<Recharge, MaximumWeaponRecharges> recharges_{};
    uint64_t effectVersion_ = 0;
    bool restored_ = false;
    beta::MeleeContact melee_;
    beta::DodgeEvasion dodge_;
    // Beta player/character IDs occupy 1..510; slot0 is the existing ground missile.
    // Eight additional disjoint bands keep simultaneous weapon shots independent.
    struct Shot {
        Projectile flight;
        beta::JudgmentContacts healing;
        combat::ProjectileContact contact;
        float contactLift = 0;
        beta::SirenContact siren;
        uint32_t target = 0;
        float range = 0;
        std::array<float, 3> facing{};
    };
    std::array<Shot, 8> shots_{};
    ThiefStance stance_{ThiefStanceRules::kCreate, ThiefStanceRules::kRemove, true};
    bool mediumKnown_ = false;
    beta::Conditions conditions_;
    beta::PhoenixReturn phoenix_;
    beta::Symbol2527Execution symbol_;
    std::string healthAfter_;
    uint16_t counterTarget_ = 0;
    std::string counterIdentity_;
    bool protectionCancelled_ = false, rocksCancelled_ = false;
};
} // namespace
std::unique_ptr<WeaponActions> CreateWorldActions(uint16_t agent) {
    if (agent < 257 || agent > 510)
        return {};
    return std::unique_ptr<WeaponActions>(new JulyActions(agent, false));
}
std::unique_ptr<WeaponActions> CreateWorldActions(uint16_t agent, const Character& saved,
                                                  const CharacterCatalog& catalog) {
    if (agent < 257 || agent > 510)
        return {};
    bool assigned = false;
    for (uint8_t byte : saved.guid.bytes)
        assigned = assigned || byte != 0;
    RosterEntry entry;
    std::string error;
    if (!assigned || !catalog.Resolve(saved, entry, error))
        return {};
    const auto& s = entry.summary;
    const auto* recipe = catalog.FindForCharacter(saved);
    InventoryState inventory;
    if (!recipe || !LoadInventoryState(saved, catalog, inventory, error))
        return {};
    std::array<Recharge, MaximumWeaponRecharges> recharges{};
    if (!SavedRecharges(saved, recharges, catalog.Skills(), recipe->skillLoadout))
        return {};
    std::unique_ptr<JulyActions> actions(new JulyActions(agent, recipe->skillLoadout != 0 || s.profession == 6));
    actions->LoadRecharges(recharges);
    actions->SetCatalog(catalog.Skills(), recipe->skillLoadout);
    uint8_t state = 0;
    if (!SavedProfessionState(saved, s.profession, s.level, state))
        return {};
    actions->SetProfession(s.profession, s.level, state);
    if (!actions->LoadEffects(saved, s.profession))
        return {};
    for (size_t i = 0; i < recipe->weapons.size(); ++i)
        if (inventory.locations[6 + i] == -1) {
            if (recipe->weapons[i].slot == 29)
                actions->SetWeaponEquipped(true);
            if (recipe->weapons[i].slot == 24)
                actions->SetAquaticWeaponEquipped(true);
        }
    return actions;
}
} // namespace july14812
} // namespace glide
