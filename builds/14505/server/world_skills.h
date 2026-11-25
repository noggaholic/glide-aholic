#pragma once
#include "glide/profile/world_actions.h"
#include "glide/profile/beta_dodge_evasion.h"
#include "glide/profile/initiative.h"
#include "glide/character.h"
#include "glide/profile/beta_missile.h"
#include "glide/profile/beta_projectile_combat.h"
#include "glide/profile/missile_launch.h"
#include "glide/profile/june_water_ground_flight.h"
#include "glide/profile/june_water_healing.h"
#include "glide/profile/beta_water_healing.h"
#include "glide/profile/beta_judgment_healing.h"
#include "glide/profile/beta_melee_combat.h"
#include "glide/profile/june_health.h"
#include "motion.h"
#include "skill_catalog.h"
#include "character_catalog.h"
#include "skill_execution.h"
#include "skill_recharge.h"
#include "game.h"
#include "weapon_projectile_fallbacks.h"
#include "thief_stance.h"
#include "glide/profile/beta_symbol_execution.h"
#include "glide/profile/beta_rock_throw.h"
#include "glide/profile/beta_phoenix_return.h"
#include "../../14422/server/weapon_projectile_fallbacks.h"
#include "../../14075/server/weapon_projectile_fallbacks.h"
#include "../../14075/server/skill_request.h"
#include <map>
#include <set>
#include <algorithm>
#include <cmath>

namespace glide {
namespace june14505 {
// Per-character phase execution and native land/water weapon selection.
// Impact effects remain separate integration work. Recharge deadlines use UTC for
// storage and a session clock for execution, like the other persisted profiles.
class WorldSkills final : public WorldActions {
  public:
    WorldSkills(SkillCatalog catalog, uint32_t loadout, uint16_t agent)
        : catalog_(std::move(catalog)), loadout_(loadout), agent_(agent), execution_(catalog_.Build()),
          stance_(ThiefStanceCreate(catalog_.Build()), ThiefStanceRemove(catalog_.Build())),
          conditions_(agent, ThiefStanceCreate(catalog_.Build()), ThiefStanceRemove(catalog_.Build())) {}
    void SetEquippedWeapons(uint8_t mask) {
        equippedWeapons_ = mask;
        if (!(mask & 2)) {
            symbol_.Cancel();
            if (active_ && active_->skill == beta::Protection965::Skill)
                protectionCancelled_ = true;
            if (active_ && active_->skill == beta::RockBarrier6224::Skill)
                rocksCancelled_ = true;
        }
    }
    bool LoadRecharges(const Character& saved, std::string& error) {
        modern::ConditionState<14505> conditions;
        const auto negative = saved.profileFields.find("condition_effects");
        if (negative != saved.profileFields.end() && !conditions.Load(negative->second)) {
            error = "Invalid saved beta conditions";
            return false;
        }
        float health = 0;
        if (beta::LegacyHealthBuild(catalog_.Build()) &&
            !modern::ReadHealthState(saved, health, beta::kJuneMaximumHealth)) {
            error = "Invalid saved June health";
            return false;
        }
        const auto profession = saved.profileFields.find("profession");
        initiativeProfession_ = profession != saved.profileFields.end() && profession->second == "5";
        const bool elemental = (catalog_.Build() == 14075 || catalog_.Build() == 14422 || catalog_.Build() == 14505) &&
                               profession != saved.profileFields.end() && profession->second == "6";
        unsigned unlockLevel = saved.level;
        uint8_t selected = 1;
        if (elemental) {
            if (!unlockLevel || unlockLevel > 80) {
                error = "Invalid June elemental level";
                return false;
            }
            if (!SavedElementalStateForBuild(catalog_.Build(), saved, 6, unlockLevel, selected)) {
                error = "Invalid saved June elemental state";
                return false;
            }
        }
        const auto resource = saved.profileFields.find("initiative_state");
        if (resource != saved.profileFields.end() && !resource->second.empty()) {
            auto ledger = modern::InitiativeLedger::ForBuild(catalog_.Build());
            if (!initiativeProfession_ || !ledger.Load(resource->second)) {
                error = "Invalid saved beta initiative";
                return false;
            }
        }
        std::map<uint32_t, uint64_t> next;
        const auto at = saved.profileFields.find("skill_recharges");
        if (at != saved.profileFields.end() && !at->second.empty()) {
            const auto& text = at->second;
            error = "Invalid saved June skill recharges";
            if (text.size() > catalog_.Candidates().size() * 32)
                return false;
            size_t begin = 0;
            while (begin < text.size()) {
                const auto end = text.find(',', begin), colon = text.find(':', begin);
                const auto stop = end == std::string::npos ? text.size() : end;
                uint64_t id = 0, deadline = 0;
                if (colon == std::string::npos || colon >= stop || !Decimal(text.substr(begin, colon - begin), id) ||
                    !id || id > UINT32_MAX || !Decimal(text.substr(colon + 1, stop - colon - 1), deadline) ||
                    !deadline || !Recharge(static_cast<uint32_t>(id)) ||
                    !next.emplace(static_cast<uint32_t>(id), deadline).second)
                    return false;
                if (end == std::string::npos)
                    break;
                begin = end + 1;
                if (begin == text.size())
                    return false;
            }
        }
        if (ThiefStanceBuild(catalog_.Build())) {
            if (!stance_.Load(saved, loadout_ == 6 ? 5 : 0)) {
                error = "Invalid saved June thief stance";
                return false;
            }
            if (saved.level && saved.level <= 80)
                level_ = saved.level;
            const auto level = saved.profileFields.find("effective_level");
            if (level != saved.profileFields.end()) {
                uint64_t value = 0;
                if (!Decimal(level->second, value) || !value || value > 80) {
                    error = "Invalid June effect level";
                    return false;
                }
                level_ = static_cast<unsigned>(value);
            }
        }
        elementalProfession_ = elemental;
        unlockLevel_ = unlockLevel;
        attunement_ = selected;
        attunementReported_ = elemental && selected != 1;
        deadlines_ = std::move(next);
        restored_ = false;
        error.clear();
        return true;
    }
    bool EvadesIncoming(uint64_t now) const override {
        return dodge_.Active(now);
    }
    bool BlocksIncoming(uint64_t now) const override {
        return active_ && active_->skill == beta::Protection965::Skill && !protectionCancelled_ && !counterTarget_ &&
               medium_ == 0 && (equippedWeapons_ & 2) && execution_.WithinPhases(1, 4, now);
    }
    bool BlockIncoming(uint16_t source, const std::string& identity, uint64_t now) override {
        if (!source || identity.empty() || !BlocksIncoming(now))
            return false;
        counterTarget_ = source;
        counterIdentity_ = identity;
        return true;
    }
    bool CounterPending() const {
        return counterTarget_ && active_ && active_->skill == 965 && !protectionCancelled_ && medium_ == 0 &&
               (equippedWeapons_ & 2);
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        if (CounterPending())
            return true;
        bool projectile = false;
        for (const auto& shot : shots_)
            if (shot.flight.phase == 2 && shot.contact.skill &&
                now > (shot.contact.sampled ? shot.contact.sampledAt : shot.flight.launchedAt))
                projectile = true;
        if (!projectile &&
            (!melee_.skill || !execution_.Active() || execution_.HasPendingPhase() || now < execution_.Due()))
            return false;
        MotionMessage message;
        return DecodeMotionForBuild(input, catalog_.Build(), message) == DecodeStatus::Complete &&
               message.canonicalId == 15;
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        if (healthRecoveryRequired_)
            return WorldActionResult::Failed;
        MotionMessage message;
        const auto decoded = DecodeMotionForBuild(input, catalog_.Build(), message);
        if (decoded == DecodeStatus::Unknown)
            return WorldActionResult::Ignored;
        if (decoded != DecodeStatus::Complete || message.consumed != input.size)
            return WorldActionResult::Failed;
        uint32_t dodgeToken = 0;
        if (message.canonicalId == 8 && message.hasOptional &&
            beta::DodgeCommand(catalog_.Build(), message.state, message.flags, message.input, dodgeToken))
            return Dodge(input, message, dodgeToken, context, out);
        if (message.canonicalId == 13)
            return Medium(message.mode, context, out);
        if (message.canonicalId != 15)
            return WorldActionResult::Ignored;
        if (message.agent != agent_)
            return WorldActionResult::Failed;
        bool ground = false;
        std::array<float, 3> groundPoint{};
        // Key release (0x39) follows the ordinary relay; it is not a cancel.
        if (catalog_.Build() == 14075) {
            april14075::SkillRequest request;
            const auto status = april14075::DecodeSkillRequest(input, request);
            if (status == DecodeStatus::Unknown)
                return WorldActionResult::Ignored;
            if (status != DecodeStatus::Complete || request.consumed != input.size)
                return WorldActionResult::Failed;
            ground = request.kind == april14075::SkillRequestKind::Ground;
            groundPoint = request.point;
        } else {
            ground = message.input.size == 14 && message.input.data[0] == 0x38;
            if (!ground && (message.input.size != 6 || message.input.data[0] != 0x37))
                return WorldActionResult::Ignored;
            if (ground) {
                std::memcpy(groundPoint.data(), message.input.data + 1, 12);
                if (!beta::MissilePoint(groundPoint))
                    return WorldActionResult::Failed;
            }
        }
        if (!Restore(context))
            return WorldActionResult::Failed;
        // Settle the original expiry and its deferred recharge before accepting
        // another key. A failed save must leave both stones and key retryable.
        if (stance_.RockDeadline() && context.wallNow >= stance_.RockDeadline())
            return Tick(context, out);
        const uint32_t identity = message.flags & 0x7fffffffu;
        if (!(message.flags & 0x80000000u) || !identity)
            return WorldActionResult::Ignored;
        const uint8_t slot = message.input.data[ground ? 13 : 5];
        if (elementalProfession_ && !ground && slot >= 12 && slot <= 15)
            return SelectElemental(static_cast<uint8_t>(slot - 11), identity, context, out);
        if (elementalProfession_ && attunement_ == 4 && medium_ == 0 && (equippedWeapons_ & 2) && !ground &&
            slot == 6 && stance_.RockCount(context.now))
            return Hurl(input, message, identity, context, out);
        const bool stanceInterrupt = ThiefStanceBuild(catalog_.Build()) && loadout_ == 6 && medium_ == 1 &&
                                     (equippedWeapons_ & 1) && !ground && message.input.data[5] == 7 &&
                                     stance_.ActiveBuff(context.now) == 8964;
        if ((execution_.Active() && !stanceInterrupt) || identity == lastIdentity_)
            return WorldActionResult::Handled;
        const auto prior = previous_.find(slot);
        const uint32_t previous = prior == previous_.end() ? 0 : prior->second;
        const bool equipped = (equippedWeapons_ & (medium_ == 1 ? 1 : 2)) != 0;
        if (!equipped && slot >= 5 && slot <= 9) {
            WorldActionOutput result;
            result.sequences = 0;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        const auto* skill = equipped ? catalog_.Select(loadout_, medium_ == 1 ? 24 : 29, slot, ElementalStateName(),
                                                       previous, previous ? 1 : 0)
                                     : nullptr;
        const bool consumeStance = ThiefStanceBuild(catalog_.Build()) && loadout_ == 6 && medium_ == 1 && equipped &&
                                   slot == 7 && stance_.ActiveBuff(context.now) == 8964;
        if (consumeStance)
            skill = &ThiefStanceFollowup(catalog_.Build());
        if (execution_.Active() && !(consumeStance && active_ && active_->skill == 8962))
            return WorldActionResult::Handled;
        if (!skill)
            return WorldActionResult::Ignored;
        const bool startStance =
            ThiefStanceBuild(catalog_.Build()) && loadout_ == 6 && medium_ == 1 && skill->skill == 8962;
        const bool changeStance = startStance || consumeStance;
        const bool swiftness = medium_ == 1 && skill->skill == beta::PersonalSwiftness4284::Skill;
        const bool changeEffects = changeStance || swiftness;
        const bool groundSkill = skill->skillFlags.find("GroundTargeted") != std::string::npos;
        const bool waterGroundFlight = ground && elementalProfession_ && attunement_ == 2 &&
                                       beta::WaterGroundFlight::Supports(catalog_.Build(), skill->skill);
        const bool groundFlight = ground && (skill->skill == 5258 || waterGroundFlight);
        const unsigned groundLaunchMs = waterGroundFlight ? beta::WaterGroundFlight::LaunchMs : 700;
        // These own water spells use their authored animation and recharge.
        // Their impact effects are separate; do not give them Fire's missile.
        const bool groundWater =
            ground && elementalProfession_ && attunement_ == 2 && (skill->skill == 111 || skill->skill == 1155);
        if (ground != groundSkill ||
            (ground && (medium_ || (!groundFlight && !groundWater) || (groundFlight && projectile_.phase))))
            return WorldActionResult::Ignored;
        if (ground) {
            const double dx = double(groundPoint[0]) - context.position.x * 32,
                         dy = double(groundPoint[1]) - context.position.y * 32,
                         dz = double(groundPoint[2]) + context.position.z * 32;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz), horizontal = std::hypot(dx, dy);
            if (!skill->hasRange || !std::isfinite(distance) || distance < skill->rangeMin ||
                distance > skill->rangeMax || horizontal <= 1) {
                WorldActionOutput rejected;
                rejected.sequences = 0;
                out = std::move(rejected);
                return WorldActionResult::Handled;
            }
            if (groundFlight && (context.now > UINT64_MAX - groundLaunchMs || !beta::MissileAgent(agent_)))
                return WorldActionResult::Failed;
        }
        if (medium_ == 2 && skill->skillFlags.find("UsableWaterSurface") == std::string::npos) {
            WorldActionOutput rejected;
            rejected.sequences = 0;
            out = std::move(rejected);
            return WorldActionResult::Handled;
        }
        const auto ready = ready_.find(skill->skill);
        if (ready != ready_.end() && context.now < ready->second)
            return WorldActionResult::Handled;
        std::vector<SkillAnimationPhase> plan;
        std::string error;
        if (!BuildSkillPlan(*skill, plan, error))
            return WorldActionResult::Failed;
        // Instant skills have no native animation order. Their effect executor
        // is separate; never invent a phase or pass an empty plan to Begin.
        if (plan.empty())
            return WorldActionResult::Ignored;
        // Animation speed is already in
        // the client's sequence asset; the native phase duration is independent
        // of that rendering multiplier (CSkillAnimationData's duration getter).
        // Native prediction maps AllowPlayerControl (Skill flag2) to state lane0x10.
        // AnimateUpperBody is a separate content flag (4), not the lane selector.
        const uint8_t actionState = skill->skillFlags.find("AllowPlayerControl") == std::string::npos ? 0x43 : 0x53;
        std::vector<SkillPhase> phases;
        bool moving = false;
        for (const auto& phase : plan) {
            SkillPhase step;
            step.index = phase.index;
            step.duration = static_cast<uint16_t>(phase.duration);
            step.type = phase.type;
            step.movement = phase.movement;
            phases.push_back(step);
            moving = moving || phase.type != 0;
        }
        SkillStage stage;
        stage.skill = skill->skill;
        stage.slot = slot;
        Reader target(ByteView(message.input.data + 1, 4));
        if ((!ground && !target.U32(stage.target)) || context.sequence == UINT32_MAX)
            return WorldActionResult::Failed;
        // Stance presses and4284's boon affect the caster. An incidental
        // selected character must not be tested against their authored range0.
        // Preserve the established no-target animation and personal effect.
        if (changeEffects || skill->skill == beta::Protection965::Skill || skill->skill == beta::RockBarrier6224::Skill)
            stage.target = 0;
        // June uses global character IDs, including the owner's own view.
        // Reject stale/foreign targets before reserving sequences or saving a
        // recharge. A rejected request must remain retryable with its identity.
        if (stage.target && stage.target != agent_ &&
            (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(stage.target))) {
            WorldActionOutput rejected;
            rejected.sequences = 0;
            out = std::move(rejected);
            return WorldActionResult::Handled;
        }
        if (stage.target && skill->hasRange) {
            CatalogPosition point = context.position;
            const bool found = stage.target == agent_ || (context.characterTargetPosition &&
                                                          context.characterTargetPosition(stage.target, point));
            const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                         z = double(point.z) - context.position.z;
            const double distance = std::sqrt(x * x + y * y + z * z) * 32;
            if (!found || !std::isfinite(distance) || distance < skill->rangeMin || distance > skill->rangeMax) {
                WorldActionOutput rejected;
                rejected.sequences = 0;
                out = std::move(rejected);
                return WorldActionResult::Handled;
            }
        }
        const auto shotRule = ProjectileRule(catalog_.Build(), skill->skill);
        size_t shotIndex = 0;
        if (shotRule.skill) {
            while (shotIndex < shots_.size() && shots_[shotIndex].flight.phase)
                ++shotIndex;
            if (stage.target == agent_ || shotIndex == shots_.size()) {
                WorldActionOutput rejected;
                rejected.sequences = 0;
                out = std::move(rejected);
                return WorldActionResult::Handled;
            }
            if (!skill->hasRange || skill->rangeMax <= 1 || !std::isfinite(skill->rangeMax) ||
                !beta::MissileAgent(agent_) || context.now > UINT64_MAX - shotRule.launchMs)
                return WorldActionResult::Failed;
        }
        stage.underwater = skill->underwater;
        std::array<float, 3> facing = context.hasFacing
                                          ? std::array<float, 3>{{context.facing.x, context.facing.y, context.facing.z}}
                                          : std::array<float, 3>{{1, 0, 0}};
        double targetDistance = std::numeric_limits<double>::infinity();
        if (moving &&
            ((stage.target == agent_) || !MovementTarget(*skill, stage.target, context, facing, targetDistance))) {
            WorldActionOutput rejected;
            rejected.sequences = 0;
            out = std::move(rejected);
            return WorldActionResult::Handled;
        }
        if (shotRule.skill) {
            const double length = std::hypot(std::hypot(double(facing[0]), double(facing[1])), double(facing[2]));
            if (!std::isfinite(length) || length <= 0)
                return WorldActionResult::Failed;
        }
        SkillExecution next(catalog_.Build());
        if (!next.Begin(stage, identity, phases, context.now))
            return WorldActionResult::Failed;
        const unsigned recharge = skill->skill == beta::RockBarrier6224::Skill ? 0 : skill->recharge;
        uint64_t duration = recharge;
        for (const auto& phase : phases)
            duration += phase.duration;
        auto deadlines = deadlines_;
        if (recharge) {
            if (!context.wallNow || context.wallNow > UINT64_MAX - duration || context.now > UINT64_MAX - duration)
                return WorldActionResult::Failed;
            for (auto it = deadlines.begin(); it != deadlines.end();)
                if (it->second <= context.wallNow)
                    it = deadlines.erase(it);
                else
                    ++it;
            deadlines[skill->skill] = context.wallNow + duration;
        }
        WorldActionOutput result, first;
        const bool cancelStance = consumeStance && execution_.Active();
        if (cancelStance && context.sequence > UINT32_MAX - 2)
            return WorldActionResult::Failed;
        if (RelayMotionForBuild(input, catalog_.Build(), agent_, context.sequence, result.owned) !=
                RelayResult::Encoded ||
            RelayMotionForBuild(input, catalog_.Build(), context.peerAgent, context.sequence, result.peers) !=
                RelayResult::Encoded)
            return WorldActionResult::Failed;
        if (cancelStance) {
            auto old = execution_;
            Buffer ownCancel, peerCancel;
            if (!old.Cancel(Pose(context, actionState_), agent_, context.peerAgent, context.sequence + 1, ownCancel,
                            peerCancel) ||
                !result.owned.Append(ownCancel.View()) || !result.peers.Append(peerCancel.View()))
                return WorldActionResult::Failed;
            result.sequences = 3;
        }
        if (next.Advance(context.now, Pose(context, actionState), agent_, context.peerAgent,
                         context.sequence + (cancelStance ? 2 : 1), first.owned, first.peers, facing,
                         targetDistance) != SkillProgress::Emitted ||
            !result.owned.Append(first.owned.View()) || !result.peers.Append(first.peers.View()))
            return WorldActionResult::Failed;
        auto symbol = symbol_;
        symbol.CancelPending();
        if (skill->skill == beta::Symbol2527::Skill && !symbol.Schedule(context))
            return WorldActionResult::Failed;
        auto pendingStance = stance_;
        Buffer effect;
        if (changeEffects) {
            if (effectVersion_ == UINT64_MAX ||
                !(swiftness ? pendingStance.StartPersonal(agent_, skill->skill, context, level_, effect)
                  : startStance
                      ? pendingStance.Start(agent_, skill->skill, context.now, context.wallNow, level_, effect)
                      : pendingStance.Consume(agent_, effect)) ||
                !result.owned.Append(effect.View()) || !result.peers.Append(effect.View()))
                return WorldActionResult::Failed;
        }
        const bool initiativeEnabled = initiativeProfession_ && bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return WorldActionResult::Failed;
        auto initiative = modern::InitiativeLedger::ForBuild(catalog_.Build());
        if (initiativeEnabled) {
            unsigned cost = 0;
            if (!modern::BetaInitialInitiativeCost(catalog_.Build(), skill->skill, cost) ||
                !initiative.Load(context.initiativeState) || !initiative.Advance(context.wallNow))
                return WorldActionResult::Failed;
            if (!initiative.Spend(cost)) {
                WorldActionOutput empty;
                empty.sequences = 0;
                out = std::move(empty);
                return WorldActionResult::Handled;
            }
            if (!modern::InitiativeCurrent(catalog_.Build(), agent_, initiative, result.owned))
                return WorldActionResult::Failed;
        }
        if (initiativeEnabled || recharge || changeEffects) {
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            const bool saved =
                initiativeEnabled
                    ? context.persistCombatInitiative(Text(deadlines), pendingStance.Text(), context.healthState,
                                                      context.conditionState, initiative.Text())
                : changeEffects
                    ? (context.persistSkillState && context.persistSkillState(Text(deadlines), pendingStance.Text()))
                    : (context.persistSkillRecharges && context.persistSkillRecharges(Text(deadlines)));
            if (!saved) {
                WorldActionOutput notice;
                notice.sequences = 0;
                Guid sender;
                const auto messages = MakeGameMessagesForBuild(catalog_.Build(), agent_);
                if (!messages || !messages->ServiceChat("The skill recharge could not be saved.", sender, notice.owned))
                    return WorldActionResult::Failed;
                out = std::move(notice);
                return WorldActionResult::Handled;
            }
            deadlines_ = std::move(deadlines);
            if (recharge)
                ready_[skill->skill] = context.now + duration;
        }
        if (initiativeEnabled)
            initiativeAnnounced_ = true;
        if (changeEffects) {
            stance_ = std::move(pendingStance);
            ++effectVersion_;
        }
        if (skill->skill == beta::Protection965::Skill) {
            protectionCancelled_ = false;
            counterTarget_ = 0;
            counterIdentity_.clear();
        }
        if (skill->skill == beta::RockBarrier6224::Skill)
            rocksCancelled_ = false;
        execution_ = std::move(next);
        symbol_ = std::move(symbol);
        actionState_ = actionState;
        active_ = skill;
        lastIdentity_ = identity;
        activeTarget_ = stage.target;
        activeFacing_ = facing;
        activeMoving_ = moving;
        melee_ = {};
        if (skill->skill != 965 && beta::MeleeCombatSkill(catalog_.Build(), skill->skill) && skill->hasRange &&
            stage.target && context.combatTarget) {
            WorldCombatTarget enemy;
            if (context.combatTarget(stage.target, enemy) && enemy.actor.agent == stage.target &&
                !enemy.actor.identity.empty()) {
                melee_.skill = skill->skill;
                melee_.identity = identity;
                melee_.target = stage.target;
                melee_.targetIdentity = enemy.actor.identity;
                melee_.minimum = skill->rangeMin;
                melee_.maximum = skill->rangeMax;
                melee_.medium = medium_;
            }
        }
        if (!effectVersion_)
            effectVersion_ = 1;
        result.peerEffectVersion = effectVersion_;
        if (groundFlight) {
            phoenix_ = {};
            // Keep the selected skill's own Projectile effect. Water uses the
            // documented exploration speed and its event900 launch anchor.
            // Fire retains its accepted mode/timing; impacts remain separate.
            projectile_ = {};
            projectile_.phase = 1;
            projectile_.identity = identity;
            projectile_.skill = skill->skill;
            projectile_.mode = waterGroundFlight ? beta::WaterGroundFlight::Mode : 1;
            projectile_.speed = waterGroundFlight ? static_cast<float>(beta::WaterGroundFlight::Speed) : 600.0f;
            projectile_.destination = groundPoint;
            projectile_.at = context.now + groundLaunchMs;
        }
        if (shotRule.skill) {
            auto& shot = shots_[shotIndex];
            shot = {};
            shot.target = stage.target;
            shot.range = skill->rangeMax;
            shot.facing = facing;
            shot.contact.Capture(catalog_.Build(), skill->skill, stage.target, context);
            auto& flight = shot.flight;
            flight.phase = 1;
            flight.identity = identity;
            flight.skill = skill->skill;
            flight.agent = static_cast<uint16_t>(beta::MissileAgent(agent_) + 256 * (shotIndex + 1));
            flight.mode = 2;
            flight.speed = shotRule.speed;
            flight.at = context.now + shotRule.launchMs;
        }
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    uint64_t PeerEffectVersion() const override {
        return effectVersion_;
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
        return beta::PreparePositiveEffects(stance_, effectVersion_, agent_, level_, c, requests, out);
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        for (const auto& shot : shots_)
            if (shot.flight.phase == 2) {
                Buffer bytes;
                if (!beta::DestroyMissile(catalog_.Build(), shot.flight.Agent(agent_), sequence, bytes) ||
                    !out.Append(bytes.View()))
                    return false;
            }
        if (projectile_.phase != 2)
            return true;
        Buffer bytes;
        return beta::DestroyMissile(catalog_.Build(), beta::MissileAgent(agent_), sequence, bytes) &&
               out.Append(bytes.View());
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        Buffer result;
        if (!seen &&
            !execution_.Snapshot(context.now, Pose(context, actionState_), context.peerAgent, context.sequence, result))
            return WorldActionResult::Failed;
        if (mediumReported_ && !WeaponSelection(medium_, result))
            return WorldActionResult::Failed;
        if (elementalProfession_ && attunementReported_ && !ElementalSelection(attunement_, result))
            return WorldActionResult::Failed;
        if (projectile_.phase == 2 && context.now < projectile_.at) {
            Buffer missile;
            if (!projectile_.Snapshot(catalog_.Build(), agent_, context.now, context.sequence, missile) ||
                !result.Append(missile.View()))
                return WorldActionResult::Failed;
        } else if (seen && projectile_.skill) {
            Buffer destroy;
            if (!beta::DestroyMissile(catalog_.Build(), beta::MissileAgent(agent_), context.sequence, destroy) ||
                !result.Append(destroy.View()))
                return WorldActionResult::Failed;
        }
        for (const auto& shot : shots_) {
            Buffer bytes;
            if (shot.flight.phase == 2 && context.now < shot.flight.at) {
                if (!shot.flight.Snapshot(catalog_.Build(), agent_, context.now, context.sequence, bytes))
                    return WorldActionResult::Failed;
            } else if (seen && shot.flight.skill) {
                if (!beta::DestroyMissile(catalog_.Build(), shot.flight.Agent(agent_), context.sequence, bytes))
                    return WorldActionResult::Failed;
            }
            if (!result.Append(bytes.View()))
                return WorldActionResult::Failed;
        }
        if (ThiefStanceBuild(catalog_.Build()) && !stance_.Snapshot(agent_, context.now, level_, result))
            return WorldActionResult::Failed;
        if (!seen && !conditions_.Snapshot(context.peerAgent, context.wallNow, level_, result))
            return WorldActionResult::Failed;
        if (!result.Size())
            return WorldActionResult::Ignored;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        if (context.wallNow && !dodge_.Advance(context.wallNow, context.skillEffects))
            return WorldActionResult::Failed;
        auto candidate = *this;
        // The catalogue copy owns different addresses until it is moved back.
        // Static follow-up definitions remain shared; rebind catalogue entries.
        for (size_t i = 0; i < catalog_.Candidates().size(); ++i)
            if (active_ == &catalog_.Candidates()[i]) {
                candidate.active_ = &candidate.catalog_.Candidates()[i];
                break;
            }
        WorldActionOutput prepared;
        bool retry = false;
        const auto status = candidate.TickPrepared(context, prepared, retry);
        if (status == WorldActionResult::Failed) {
            if (candidate.healthRecoveryRequired_)
                healthRecoveryRequired_ = true;
            return status;
        }
        if (!retry)
            *this = std::move(candidate);
        else
            beta::KeepProjectileSamples(shots_, candidate.shots_);
        if (status == WorldActionResult::Handled)
            out = std::move(prepared);
        return status;
    }
    WorldActionResult TickPrepared(const WorldActionContext& context, WorldActionOutput& out, bool& discardTick) {
        if (healthRecoveryRequired_)
            return WorldActionResult::Failed;
        if (CounterPending())
            return StartCounter(context, out);
        if (melee_.skill) {
            if (!active_ || active_->skill != melee_.skill || lastIdentity_ != melee_.identity ||
                medium_ != melee_.medium || !(equippedWeapons_ & (medium_ == 1 ? 1 : 2))) {
                melee_ = {};
                out = {};
                out.sequences = 0;
                return WorldActionResult::Handled;
            }
            if (execution_.Active() && !execution_.HasPendingPhase() && context.now >= execution_.Due()) {
                const int hit = melee_.Settle(catalog_.Build(), agent_, level_, context, out);
                if (hit < 0) {
                    healthRecoveryRequired_ = true;
                    return WorldActionResult::Failed;
                }
                if (!hit)
                    discardTick = true;
                else {
                    // These six native auto-attack stages have no recharge.
                    // Their final phase has already elapsed; finish the chain
                    // here so the next native input can be accepted immediately.
                    if (active_->skill != 965) {
                        previous_[active_->slot] = active_->skill;
                        execution_.Reset();
                        active_ = nullptr;
                        activeTarget_ = 0;
                        activeMoving_ = false;
                    }
                }
                return WorldActionResult::Handled;
            }
        }
        if (!Restore(context))
            return WorldActionResult::Failed;
        const int synced =
            beta::SyncConditions(conditions_, level_, effectVersion_, Text(deadlines_), stance_, context, out);
        if (synced < 0)
            return WorldActionResult::Failed;
        if (synced == 0) {
            discardTick = true;
            return WorldActionResult::Handled;
        }
        if (synced == 2)
            return WorldActionResult::Handled;
        if (phoenix_.Due(projectile_, context)) {
            if (stance_.HasRegeneration()) {
                const auto before = stance_.Text();
                WorldActionOutput positive;
                positive.sequences = 0;
                const int advanced = AdvanceStance(context, positive);
                if (advanced < 0)
                    return WorldActionResult::Failed;
                if (!advanced || before != stance_.Text() || positive.owned.Size()) {
                    out = std::move(positive);
                    return WorldActionResult::Handled;
                }
            }
            const int returned = beta::AdvancePhoenix(catalog_.Build(), agent_, level_, projectile_, phoenix_, stance_,
                                                      conditions_, effectVersion_, Text(deadlines_), context, out);
            if (returned < 0)
                return WorldActionResult::Failed;
            if (!returned) {
                discardTick = true;
                return WorldActionResult::Handled;
            }
            if (returned == 2)
                return WorldActionResult::Handled;
        }
        const bool protection = ProtectionOutcomeDue(context), rocks = RockOutcomeDue(context);
        if (protection || rocks ||
            (projectile_.phase == 2 && projectile_.skill == beta::WaterGroundHealing::Skill &&
             context.now >= projectile_.at && beta::LegacyHealthBuild(catalog_.Build()))) {
            if (stance_.HasRegeneration()) {
                const auto before = stance_.Text();
                WorldActionOutput positive;
                positive.sequences = 0;
                const int advanced = AdvanceStance(context, positive);
                if (advanced < 0)
                    return WorldActionResult::Failed;
                if (!advanced || before != stance_.Text() || positive.owned.Size()) {
                    out = std::move(positive);
                    return WorldActionResult::Handled;
                }
            }
            return rocks        ? FinishRocks(context, out)
                   : protection ? FinishProtection(context, out)
                                : FinishWaterHealing(context, out);
        }
        const int judgment = beta::AdvanceJudgmentHealing(catalog_.Build(), agent_, level_, shots_, effectVersion_,
                                                          healthRecoveryRequired_, context, out);
        if (judgment < 0)
            return WorldActionResult::Failed;
        if (!judgment) {
            discardTick = true;
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        if (judgment == 2)
            return WorldActionResult::Handled;
        const int projectileHit = beta::AdvanceProjectileCombat(catalog_.Build(), agent_, shots_, effectVersion_,
                                                                healthRecoveryRequired_, context, out);
        if (projectileHit < 0)
            return WorldActionResult::Failed;
        if (!projectileHit) {
            discardTick = true;
            out = {};
            out.sequences = 0;
            return WorldActionResult::Handled;
        }
        if (projectileHit == 2)
            return WorldActionResult::Handled;
        WorldActionOutput result;
        PositiveSave positive;
        const int effectProgress = AdvanceStance(context, result, &positive);
        if (effectProgress < 0)
            return WorldActionResult::Failed;
        if (!effectProgress) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        for (const auto& entry : resume_) {
            const auto due = ready_.find(entry.first);
            const auto remaining = due != ready_.end() && due->second > context.now ? due->second - context.now : 0;
            Buffer report;
            if (!(remaining
                      ? EncodeSkillRechargeForBuild(
                            catalog_.Build(), agent_, entry.first,
                            static_cast<uint32_t>((std::min)(remaining, uint64_t(entry.second))), entry.second, report)
                      : EncodeSkillRechargeClearForBuild(catalog_.Build(), agent_, entry.first, report)) ||
                !result.owned.Append(report.View()))
                return WorldActionResult::Failed;
        }
        Buffer ownStage, peerStage;
        auto facing = activeFacing_;
        double targetDistance = std::numeric_limits<double>::infinity();
        const bool cancelled = execution_.HasPendingPhase() && activeMoving_ && context.now >= execution_.Due() &&
                               !MovementTarget(*active_, activeTarget_, context, facing, targetDistance);
        SkillProgress progress;
        if (cancelled) {
            if (!execution_.Cancel(Pose(context, actionState_), agent_, context.peerAgent, context.sequence, ownStage,
                                   peerStage))
                return WorldActionResult::Failed;
            progress = SkillProgress::Completed;
        } else
            progress = execution_.Advance(context.now, Pose(context, actionState_), agent_, context.peerAgent,
                                          context.sequence, ownStage, peerStage, facing, targetDistance);
        if (!result.owned.Append(ownStage.View()) || !result.peers.Append(peerStage.View()))
            return WorldActionResult::Failed;
        if (progress == SkillProgress::Invalid)
            return WorldActionResult::Failed;
        if (progress == SkillProgress::Completed) {
            // The deadline was saved before the first phase. A delayed worker
            // cannot extend it beyond what a reconnect would restore.
            if (active_->recharge) {
                Buffer recharge;
                const auto due = ready_.find(active_->skill);
                const uint64_t remaining =
                    due != ready_.end() && due->second > context.now ? due->second - context.now : 0;
                if (!EncodeSkillRechargeForBuild(
                        catalog_.Build(), agent_, active_->skill,
                        static_cast<uint32_t>((std::min)(remaining, uint64_t(active_->recharge))), active_->recharge,
                        recharge) ||
                    !result.owned.Append(recharge.View()))
                    return WorldActionResult::Failed;
            }
            if (cancelled)
                previous_.erase(active_->slot);
            else
                previous_[active_->slot] = active_->skill;
            active_ = nullptr;
            activeTarget_ = 0;
            activeMoving_ = false;
        }
        result.sequences = progress == SkillProgress::Emitted || cancelled ? 1 : 0;
        if (projectile_.phase && context.now >= projectile_.at) {
            if (effectVersion_ == UINT64_MAX || context.sequence > UINT32_MAX - result.sequences)
                return WorldActionResult::Failed;
            const auto pose = Pose(context, 0);
            const std::array<float, 3> position{{pose.pose[0], pose.pose[1], pose.pose[2]}};
            auto next = projectile_;
            Buffer bytes;
            if (!next.Advance(catalog_.Build(), agent_, position, context.now, context.sequence + result.sequences,
                              bytes) ||
                !result.owned.Append(bytes.View()) || !result.peers.Append(bytes.View()))
                return WorldActionResult::Failed;
            projectile_ = std::move(next);
            if (bytes.Size()) {
                ++result.sequences;
                result.peerEffectVersion = ++effectVersion_;
            }
        }
        if (!AdvanceShots(context, result))
            return WorldActionResult::Failed;
        for (auto entry = ready_.begin(); entry != ready_.end();) {
            if (context.now < entry->second) {
                ++entry;
                continue;
            }
            Buffer clear;
            if (!EncodeSkillRechargeClearForBuild(catalog_.Build(), agent_, entry->first, clear) ||
                !result.owned.Append(clear.View()))
                return WorldActionResult::Failed;
            entry = ready_.erase(entry);
        }
        resume_.clear();
        bool reserved = false;
        if (positive.required) {
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            reserved = true;
            const bool saved =
                positive.initiative ? context.persistCombatInitiative(Text(deadlines_), stance_.Text(), positive.health,
                                                                      context.conditionState, positive.resource)
                : stance_.HasRegeneration()
                    ? (context.persistCombatState &&
                       context.persistCombatState(Text(deadlines_), stance_.Text(), positive.health))
                    : (context.persistSkillState && context.persistSkillState(Text(deadlines_), stance_.Text()));
            if (!saved) {
                discardTick = true;
                WorldActionOutput empty;
                empty.sequences = 0;
                out = std::move(empty);
                return WorldActionResult::Handled;
            }
        }
        const int symbol = symbol_.Advance(catalog_.Build(), agent_, level_, stance_, effectVersion_,
                                           healthRecoveryRequired_, Text(deadlines_), context, result, reserved);
        if (symbol < 0)
            return WorldActionResult::Failed;
        if (progress == SkillProgress::Waiting && !result.owned.Size() && symbol != 2 && !reserved)
            return WorldActionResult::Ignored;
        out = std::move(result);
        return WorldActionResult::Handled;
    }

  private:
    struct PositiveSave {
        bool required = false, initiative = false;
        std::string health, resource;
    };
    bool RockRecharge(uint64_t start, const WorldActionContext& c, Buffer& bytes, std::map<uint32_t, uint64_t>& ready,
                      std::map<uint32_t, uint64_t>& deadlines) const {
        using R = beta::RockBarrier6224;
        if (!start || !c.wallNow || start > UINT64_MAX - R::RechargeMs)
            return false;
        const auto end = start + R::RechargeMs, remaining = end > c.wallNow ? end - c.wallNow : 0;
        if (c.now > UINT64_MAX - remaining)
            return false;
        if (remaining) {
            ready[R::Skill] = c.now + remaining;
            deadlines[R::Skill] = end;
        } else {
            ready.erase(R::Skill);
            deadlines.erase(R::Skill);
        }
        Buffer recharge;
        // UTC and the session clock are sampled independently. A due session
        // expiry may precede its UTC deadline by one timer quantum. Keep that
        // original durable deadline and clamp only the native display value.
        return EncodeSkillRechargeForBuild(catalog_.Build(), agent_, R::Skill,
                                           static_cast<uint32_t>((std::min)(remaining, uint64_t(R::RechargeMs))),
                                           R::RechargeMs, recharge) &&
               bytes.Append(recharge.View());
    }
    bool RockOutcomeDue(const WorldActionContext& c) const {
        return active_ && active_->skill == beta::RockBarrier6224::Skill && !rocksCancelled_ && medium_ == 0 &&
               (equippedWeapons_ & 2) && execution_.Active() && !execution_.HasPendingPhase() &&
               c.now >= execution_.Due();
    }
    WorldActionResult FinishRocks(const WorldActionContext& c, WorldActionOutput& out) {
        auto effects = stance_;
        auto next = execution_;
        auto previous = previous_;
        WorldActionOutput result;
        result.sequences = 0;
        if (effectVersion_ == UINT64_MAX || next.Advance(c.now, Pose(c, actionState_), agent_, c.peerAgent, c.sequence,
                                                         result.owned, result.peers) != SkillProgress::Completed)
            return WorldActionResult::Failed;
        Buffer effect;
        if (!effects.StartRocks(agent_, c, level_, effect) || !result.owned.Append(effect.View()) ||
            !result.peers.Append(effect.View()))
            return WorldActionResult::Failed;
        previous[active_->slot] = active_->skill;
        if (!c.ReserveSequences(0))
            return WorldActionResult::Failed;
        if (!c.persistSkillState || !c.persistSkillState(Text(deadlines_), effects.Text())) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        stance_ = std::move(effects);
        execution_ = std::move(next);
        previous_ = std::move(previous);
        active_ = nullptr;
        activeTarget_ = 0;
        activeMoving_ = false;
        result.peerEffectVersion = ++effectVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Hurl(ByteView input, const MotionMessage& message, uint32_t identity, const WorldActionContext& c,
                           WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        if (identity == lastInstantIdentity_ || identity == lastIdentity_) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        Reader reader(ByteView(message.input.data + 1, 4));
        uint32_t target = 0;
        if (!reader.U32(target) || effectVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        auto effects = stance_;
        auto shots = shots_;
        auto ready = ready_;
        auto deadlines = deadlines_;
        Buffer effect;
        const int prepared = beta::PrepareRockThrow(agent_, identity, target, c, shots, effects, effect);
        for (auto& shot : shots)
            if (shot.flight.phase == 1 && shot.flight.identity == identity)
                shot.contact.Capture(catalog_.Build(), shot.flight.skill, target, c);
        if (prepared < 0)
            return WorldActionResult::Failed;
        if (!prepared) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (RelayMotionForBuild(input, catalog_.Build(), agent_, c.sequence, result.owned) != RelayResult::Encoded ||
            RelayMotionForBuild(input, catalog_.Build(), c.peerAgent, c.sequence, result.peers) !=
                RelayResult::Encoded ||
            !result.owned.Append(effect.View()) || !result.peers.Append(effect.View()))
            return WorldActionResult::Failed;
        if (!effects.RockCount(c.now) && !RockRecharge(c.wallNow, c, result.owned, ready, deadlines))
            return WorldActionResult::Failed;
        if (!c.ReserveSequences(1))
            return WorldActionResult::Failed;
        if (!c.persistSkillState || !c.persistSkillState(Text(deadlines), effects.Text())) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        stance_ = std::move(effects);
        shots_ = std::move(shots);
        ready_ = std::move(ready);
        deadlines_ = std::move(deadlines);
        lastInstantIdentity_ = identity;
        result.sequences = 1;
        result.peerEffectVersion = ++effectVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool ProtectionOutcomeDue(const WorldActionContext& c) const {
        return c.persistPreparedAlliedEffects && active_ && active_->skill == beta::Protection965::Skill &&
               !protectionCancelled_ && medium_ == 0 && (equippedWeapons_ & 2) &&
               execution_.PhaseDue(beta::Protection965::OutcomePhase, c.now);
    }
    WorldActionResult Dodge(ByteView input, const MotionMessage& message, uint32_t token, const WorldActionContext& c,
                            WorldActionOutput& out) {
        out = {};
        out.sequences = 0;
        if (message.agent != agent_ || c.peerAgent != agent_)
            return WorldActionResult::Failed;
        float health = 0;
        if (!modern::ReadHealthState(c.healthState, health, beta::BetaHealthMaximum(catalog_.Build())))
            return WorldActionResult::Failed;
        auto next = *this;
        if (health <= 0 || !next.dodge_.Accept(token, c.now, c.wallNow, c.skillEffects))
            return WorldActionResult::Handled;
        WorldActionOutput result;
        result.sequences = 0;
        if (next.execution_.Active()) {
            if (!next.execution_.Cancel(Pose(c, actionState_), agent_, c.peerAgent, c.sequence, result.owned,
                                        result.peers))
                return WorldActionResult::Failed;
            ++result.sequences;
            if (active_ && active_->recharge) {
                const auto at = ready_.find(active_->skill);
                const uint64_t remaining = at != ready_.end() && at->second > c.now ? at->second - c.now : 0;
                Buffer recharge;
                if (!EncodeSkillRechargeForBuild(
                        catalog_.Build(), agent_, active_->skill,
                        static_cast<uint32_t>((std::min)(remaining, uint64_t(active_->recharge))), active_->recharge,
                        recharge) ||
                    !result.owned.Append(recharge.View()))
                    return WorldActionResult::Failed;
            }
        }
        if (c.sequence > UINT32_MAX - result.sequences)
            return WorldActionResult::Failed;
        Buffer own, peer;
        const auto sequence = c.sequence + result.sequences;
        if (RelayMotionForBuild(input, catalog_.Build(), agent_, sequence, own) != RelayResult::Encoded ||
            RelayMotionForBuild(input, catalog_.Build(), c.peerAgent, sequence, peer) != RelayResult::Encoded ||
            !result.owned.Append(own.View()) || !result.peers.Append(peer.View()) ||
            !c.ReserveSequences(++result.sequences))
            return WorldActionResult::Failed;
        next.active_ = nullptr;
        next.activeTarget_ = 0;
        next.activeMoving_ = false;
        next.previous_.clear();
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
        *this = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult StartCounter(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = execution_;
        WorldActionOutput result;
        result.sequences = 1;
        if (!next.ExitLoop(3, 4, c.now, 0) ||
            next.Advance(c.now, Pose(c, actionState_), agent_, c.peerAgent, c.sequence, result.owned, result.peers,
                         activeFacing_) != SkillProgress::Emitted ||
            !c.ReserveSequences(1))
            return WorldActionResult::Failed;
        execution_ = std::move(next);
        melee_ = {};
        melee_.skill = 965;
        melee_.identity = lastIdentity_;
        melee_.maximum = 170;
        melee_.medium = medium_;
        protectionCancelled_ = true;
        counterTarget_ = 0;
        counterIdentity_.clear();
        activeTarget_ = 0;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult FinishProtection(const WorldActionContext& c, WorldActionOutput& out) {
        auto next = execution_;
        WorldActionOutput result;
        result.sequences = 1;
        if (next.Advance(c.now, Pose(c, actionState_), agent_, c.peerAgent, c.sequence, result.owned, result.peers,
                         activeFacing_) != SkillProgress::Emitted)
            return WorldActionResult::Failed;
        beta::Protection965Execution outcome;
        if (!outcome.Schedule(c))
            return WorldActionResult::Failed;
        const int applied = outcome.Advance(catalog_.Build(), agent_, level_, stance_, effectVersion_,
                                            healthRecoveryRequired_, Text(deadlines_), c, result);
        if (applied < 0)
            return WorldActionResult::Failed;
        if (applied != 2) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        execution_ = std::move(next);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult FinishWaterHealing(const WorldActionContext& context, WorldActionOutput& out) {
        beta::PreparedWaterHealing prepared;
        if (!beta::PrepareWaterHealing(catalog_.Build(), agent_, projectile_.destination, context, prepared))
            return WorldActionResult::Failed;
        const bool surfaceImpact = catalog_.Build() != 14075;
        WorldActionOutput result;
        result.sequences = static_cast<uint32_t>(prepared.healedAgents.size()) + (surfaceImpact ? 2u : 1u);
        if (context.sequence > UINT32_MAX - result.sequences || !result.owned.Append(prepared.owned.View()) ||
            !result.peers.Append(prepared.peers.View()))
            return WorldActionResult::Failed;
        uint32_t sequence = context.sequence;
        for (uint16_t healed : prepared.healedAgents) {
            Buffer hit;
            if (!beta::BetaMissileHit(catalog_.Build(), projectile_.Agent(agent_), sequence++, projectile_.identity,
                                      healed, hit) ||
                !result.owned.Append(hit.View()) || !result.peers.Append(hit.View()))
                return WorldActionResult::Failed;
        }
        if (surfaceImpact) {
            Buffer surface;
            if (!beta::JuneMissileSurfaceImpact(catalog_.Build(), projectile_.Agent(agent_), sequence++,
                                                projectile_.identity, surface) ||
                !result.owned.Append(surface.View()) || !result.peers.Append(surface.View()))
                return WorldActionResult::Failed;
        }
        auto next = projectile_;
        Buffer destroy;
        const std::array<float, 3> position{{0, 0, 0}}; // Expiry does not use a launch position.
        if (effectVersion_ == UINT64_MAX ||
            !next.Advance(catalog_.Build(), agent_, position, context.now, sequence, destroy) ||
            !result.owned.Append(destroy.View()) || !result.peers.Append(destroy.View()))
            return WorldActionResult::Failed;
        if (!context.ReserveSequences(result.sequences))
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
        projectile_ = std::move(next);
        result.peerEffectVersion = ++effectVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    // Existing HoT durable effect lifecycle, restricted to the one own June pair.
    int AdvanceStance(const WorldActionContext& context, WorldActionOutput& out, PositiveSave* deferred = nullptr) {
        if (!ThiefStanceBuild(catalog_.Build()))
            return 1;
        auto pending = stance_;
        auto ready = ready_;
        auto deadlines = deadlines_;
        const auto rockDeadline = pending.RockDeadline();
        Buffer effect;
        bool changed = false, clockChanged = false;
        std::string health;
        if (!pending.PrepareHealth(catalog_.Build(), context, health, clockChanged))
            return -1;
        const bool unavailable =
            pending.ActiveStance(context.now) && ((mediumKnown_ && medium_ != 1) || !(equippedWeapons_ & 1));
        if (unavailable) {
            if (!pending.Consume(agent_, effect))
                return -1;
            changed = true;
        } else if (!pending.Advance(agent_, context.now, level_, effect, changed))
            return -1;
        if (rockDeadline && !pending.RockDeadline() &&
            !RockRecharge(rockDeadline, context, out.owned, ready, deadlines))
            return -1;
        changed = changed || health != context.healthState;
        if (changed && effectVersion_ == UINT64_MAX)
            return -1;
        if (!out.owned.Append(effect.View()) || !out.peers.Append(effect.View()))
            return -1;
        if (health != context.healthState) {
            float current = 0;
            Buffer bytes;
            if (!modern::ReadHealthState(health, current, beta::BetaHealthMaximum(catalog_.Build())) ||
                !beta::BetaHealthUpdate(catalog_.Build(), agent_, current, bytes) || !out.owned.Append(bytes.View()) ||
                !out.peers.Append(bytes.View()))
                return -1;
        }
        const bool initiativeEnabled = initiativeProfession_ && bool(context.persistCombatInitiative);
        if (!context.initiativeState.empty() && !initiativeEnabled)
            return -1;
        auto initiative = modern::InitiativeLedger::ForBuild(catalog_.Build());
        if (initiativeEnabled && !initiative.Load(context.initiativeState))
            return -1;
        const unsigned previous = initiative.Points();
        if (initiativeEnabled && !initiative.Advance(context.wallNow))
            return -1;
        const bool initiativeSave = initiativeEnabled && (!initiativeAnnounced_ || context.initiativeState.empty() ||
                                                          previous != initiative.Points());
        if (initiativeSave && !modern::InitiativeCurrent(catalog_.Build(), agent_, initiative, out.owned))
            return -1;
        if (initiativeSave || pending.Text() != stance_.Text()) {
            // Keep the existing locked effect-save path. Later phases may
            // still emit motion sequences; reserving zero here would prevent
            // the server from admitting that phase output after a successful save.
            if (deferred) {
                deferred->required = true;
                deferred->initiative = initiativeEnabled;
                deferred->health = health;
                deferred->resource = initiative.Text();
            } else {
                const bool saved =
                    initiativeEnabled ? context.persistCombatInitiative(Text(deadlines), pending.Text(), health,
                                                                        context.conditionState, initiative.Text())
                    : pending.HasRegeneration()
                        ? (context.persistCombatState &&
                           context.persistCombatState(Text(deadlines), pending.Text(), health))
                        : (context.persistSkillState && context.persistSkillState(Text(deadlines), pending.Text()));
                if (!saved)
                    return 0;
            }
        }
        if (initiativeSave)
            initiativeAnnounced_ = true;
        stance_ = std::move(pending);
        ready_ = std::move(ready);
        deadlines_ = std::move(deadlines);
        if (changed)
            out.peerEffectVersion = ++effectVersion_;
        return 1;
    }
    // Own timing and original velocity take precedence over explicit compatible defaults.
    static WeaponProjectile ProjectileRule(unsigned build, unsigned skill) {
        if (build == 14505) {
            if (const auto* r = FindWeaponProjectileRule(skill))
                return *r;
        }
        if (build == 14422) {
            if (const auto* r = june14422::FindWeaponProjectileRule(skill))
                return {r->skill, r->launchMs, r->speed};
        }
        if (build == 14075) {
            if (const auto* r = april14075::FindWeaponProjectileRule(skill))
                return {r->skill, r->launchMs, r->speed};
        }
        return {};
    }
    bool AdvanceShots(const WorldActionContext& context, WorldActionOutput& out) {
        for (auto& shot : shots_) {
            if (!shot.flight.phase || context.now < shot.flight.at)
                continue;
            if (shot.flight.phase == 2 && shot.contact.skill && context.now >= shot.flight.at)
                continue;
            auto flight = shot.flight;
            const auto pose = Pose(context, 0);
            std::array<float, 3> start{{pose.pose[0], pose.pose[1], pose.pose[2]}};
            if (flight.phase == 1) {
                if (!shot.contact.Matches(context)) {
                    shot.flight.phase = 0;
                    shot.contact = {};
                    continue;
                }
                shot.contactLift = medium_ == 0 ? 39.0f : 0.0f;
                if (shot.target) {
                    CatalogPosition point;
                    if (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(shot.target) ||
                        !context.characterTargetPosition || !context.characterTargetPosition(shot.target, point)) {
                        shot.flight.phase = 0;
                        continue;
                    }
                    flight.destination = {{point.x * 32, point.y * 32, -point.z * 32}};
                    const double dx = double(flight.destination[0]) - start[0],
                                 dy = double(flight.destination[1]) - start[1],
                                 dz = double(flight.destination[2]) - start[2],
                                 distance = std::sqrt(dx * dx + dy * dy + dz * dz);
                    if (!std::isfinite(distance) || distance <= 1 || distance > shot.range) {
                        shot.flight.phase = 0;
                        continue;
                    }
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
            if (effectVersion_ == UINT64_MAX || context.sequence > UINT32_MAX - out.sequences)
                return false;
            Buffer bytes;
            if (!flight.Advance(catalog_.Build(), agent_, start, context.now, context.sequence + out.sequences,
                                bytes) ||
                !out.owned.Append(bytes.View()) || !out.peers.Append(bytes.View()))
                return false;
            shot.flight = std::move(flight);
            if (bytes.Size()) {
                ++out.sequences;
                out.peerEffectVersion = ++effectVersion_;
            }
        }
        return true;
    }
    bool WeaponSelection(uint8_t medium, Buffer& out) const {
        return out.U16(WorldMessage(catalog_.Build(), 129)) && out.VarU32(agent_) && out.U8(medium == 1 ? 0 : 4);
    }
    const char* ElementalStateName() const {
        static const char* states[] = {"None", "ElementalAttunementFire", "ElementalAttunementWater",
                                       "ElementalAttunementAir", "ElementalAttunementEarth"};
        return states[attunement_];
    }
    bool ElementalSelection(uint8_t state, Buffer& out) const {
        return out.U16(WorldMessage(catalog_.Build(), 216)) && out.VarU32(agent_) && out.VarU32(state);
    }
    WorldActionResult SelectElemental(uint8_t state, uint32_t identity, const WorldActionContext& context,
                                      WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        if (identity == lastIdentity_ || (state >= 3 && unlockLevel_ < 3) || (state >= 4 && unlockLevel_ < 6)) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (state == attunement_) {
            lastIdentity_ = identity;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (!context.persistProfessionState) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (effectVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        auto execution = execution_;
        if (execution.Active()) {
            if (!execution.Cancel(Pose(context, actionState_), agent_, context.peerAgent, context.sequence,
                                  result.owned, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
        }
        if (!ElementalSelection(state, result.owned) || !ElementalSelection(state, result.peers) ||
            !context.ReserveSequences(result.sequences))
            return WorldActionResult::Failed;
        if (!context.persistProfessionState(state)) {
            WorldActionOutput retry;
            retry.sequences = 0;
            out = std::move(retry);
            return WorldActionResult::Handled;
        }
        execution_ = std::move(execution);
        active_ = nullptr;
        activeTarget_ = 0;
        activeMoving_ = false;
        previous_.clear();
        if (projectile_.phase == 1)
            projectile_.phase = 0;
        for (auto& shot : shots_)
            if (shot.flight.phase == 1)
                shot.flight.phase = 0;
        attunement_ = state;
        attunementReported_ = true;
        lastIdentity_ = identity;
        result.peerEffectVersion = ++effectVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Medium(uint8_t mode, const WorldActionContext& context, WorldActionOutput& out) {
        // Original April/June Agent state, not July's direct collider state:
        // 0..4 land/air, 5/6 deep water, 7/8 surface, 9 unknown.
        if (mode >= 9)
            return WorldActionResult::Ignored;
        const uint8_t medium = mode >= 7 ? 2 : mode >= 5 ? 1 : 0;
        if (medium == medium_) {
            if (ThiefStanceBuild(catalog_.Build()))
                mediumKnown_ = true;
            return WorldActionResult::Ignored;
        }
        if (effectVersion_ == UINT64_MAX)
            return WorldActionResult::Failed;
        WorldActionOutput result;
        result.sequences = 0;
        auto execution = execution_;
        if (execution.Active()) {
            if (!execution.Cancel(Pose(context, actionState_), agent_, context.peerAgent, context.sequence,
                                  result.owned, result.peers))
                return WorldActionResult::Failed;
            result.sequences = 1;
            if (active_->recharge) {
                const auto due = ready_.find(active_->skill);
                const uint64_t remaining =
                    due != ready_.end() && due->second > context.now ? due->second - context.now : 0;
                Buffer recharge;
                if (!EncodeSkillRechargeForBuild(
                        catalog_.Build(), agent_, active_->skill,
                        static_cast<uint32_t>((std::min)(remaining, uint64_t(active_->recharge))), active_->recharge,
                        recharge) ||
                    !result.owned.Append(recharge.View()))
                    return WorldActionResult::Failed;
            }
        }
        if (projectile_.phase == 2) {
            if (context.sequence > UINT32_MAX - result.sequences)
                return WorldActionResult::Failed;
            Buffer destroy;
            if (!beta::DestroyMissile(catalog_.Build(), beta::MissileAgent(agent_), context.sequence + result.sequences,
                                      destroy) ||
                !result.owned.Append(destroy.View()) || !result.peers.Append(destroy.View()))
                return WorldActionResult::Failed;
            ++result.sequences;
        }
        for (const auto& shot : shots_)
            if (shot.flight.phase == 2) {
                if (context.sequence > UINT32_MAX - result.sequences)
                    return WorldActionResult::Failed;
                Buffer bytes;
                if (!beta::DestroyMissile(catalog_.Build(), shot.flight.Agent(agent_),
                                          context.sequence + result.sequences, bytes) ||
                    !result.owned.Append(bytes.View()) || !result.peers.Append(bytes.View()))
                    return WorldActionResult::Failed;
                ++result.sequences;
            }
        if (!WeaponSelection(medium, result.owned) || !WeaponSelection(medium, result.peers))
            return WorldActionResult::Failed;
        auto pendingStance = stance_;
        // The first deep-water report after restoration confirms the stance's
        // valid medium; only leaving deep water invalidates this aquatic pair.
        const bool consume = ThiefStanceBuild(catalog_.Build()) && medium != 1 && stance_.ActiveStance(context.now);
        if (consume) {
            Buffer effect;
            if (!pendingStance.Consume(agent_, effect) || !result.owned.Append(effect.View()) ||
                !result.peers.Append(effect.View()))
                return WorldActionResult::Failed;
            if (!context.ReserveSequences(result.sequences))
                return WorldActionResult::Failed;
            if (!context.persistSkillState || !context.persistSkillState(Text(deadlines_), pendingStance.Text())) {
                WorldActionOutput retry;
                retry.sequences = 0;
                out = std::move(retry);
                return WorldActionResult::Handled;
            }
        }
        stance_ = std::move(pendingStance);
        execution_ = std::move(execution);
        active_ = nullptr;
        activeTarget_ = 0;
        activeMoving_ = false;
        previous_.clear();
        for (auto& shot : shots_)
            shot.flight.phase = 0;
        projectile_.phase = 0;
        phoenix_ = {};
        symbol_.Cancel();
        medium_ = medium;
        mediumReported_ = true;
        if (ThiefStanceBuild(catalog_.Build()))
            mediumKnown_ = true;
        result.peerEffectVersion = ++effectVersion_;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    static bool MovementTarget(const SkillCandidate& skill, uint32_t target, const WorldActionContext& context,
                               std::array<float, 3>& facing, double& distance) {
        if (!target)
            return true;
        CatalogPosition point;
        if (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(target) ||
            !context.characterTargetPosition || !context.characterTargetPosition(target, point))
            return false;
        const double dx = double(point.x) - context.position.x, dy = double(point.y) - context.position.y,
                     dz = double(point.z) - context.position.z;
        const double range = std::sqrt(dx * dx + dy * dy + dz * dz) * 32;
        if (!std::isfinite(range) || (skill.hasRange && (range < skill.rangeMin || range > skill.rangeMax)))
            return false;
        distance = std::sqrt(dx * dx + dy * dy + (skill.underwater ? dz * dz : 0)) * 32;
        if (distance > 0.001)
            facing = {{static_cast<float>(dx), static_cast<float>(dy), skill.underwater ? static_cast<float>(-dz) : 0}};
        return true;
    }
    static bool Decimal(const std::string& text, uint64_t& out) {
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
    uint32_t Recharge(uint32_t id) const {
        for (const auto& candidate : catalog_.Candidates())
            if (candidate.loadout == loadout_ && candidate.skill == id)
                return candidate.recharge;
        return 0;
    }
    static std::string Text(const std::map<uint32_t, uint64_t>& entries) {
        std::string result;
        for (const auto& entry : entries) {
            if (!result.empty())
                result += ',';
            result += std::to_string(entry.first) + ':' + std::to_string(entry.second);
        }
        return result;
    }
    bool Restore(const WorldActionContext& context) {
        if (restored_)
            return true;
        std::map<uint32_t, uint64_t> ready;
        std::map<uint32_t, uint32_t> reports;
        for (const auto& entry : deadlines_) {
            if (!context.wallNow)
                return false;
            const uint64_t remaining = entry.second > context.wallNow ? entry.second - context.wallNow : 0;
            // Include the bounded phase plan when restoring during an action.
            if (remaining > uint64_t(Recharge(entry.first)) + 60000 || context.now > UINT64_MAX - remaining)
                return false;
            if (remaining)
                ready[entry.first] = context.now + remaining;
            reports[entry.first] = Recharge(entry.first);
        }
        if (ThiefStanceBuild(catalog_.Build()) && !stance_.Restore(context.now, context.wallNow))
            return false;
        ready_ = std::move(ready);
        resume_ = std::move(reports);
        restored_ = true;
        return true;
    }
    static CharacterAction Pose(const WorldActionContext& context, uint8_t state) {
        CharacterAction action;
        action.state = state;
        action.pose = {{context.position.x * 32, context.position.y * 32, -context.position.z * 32, 0}};
        return action;
    }
    SkillCatalog catalog_;
    uint32_t loadout_ = 0, lastIdentity_ = 0;
    uint16_t agent_ = 0;
    uint8_t actionState_ = 0;
    uint8_t equippedWeapons_ = 3;
    uint8_t medium_ = 0;
    bool mediumReported_ = false, mediumKnown_ = false;
    uint64_t effectVersion_ = 0;
    beta::MissileFlight projectile_;
    beta::MeleeContact melee_;
    beta::DodgeEvasion dodge_;
    bool healthRecoveryRequired_ = false;
    // Ground missile owns band0; eight additional owner-disjoint bands match July.
    struct Shot {
        beta::MissileFlight flight;
        beta::JudgmentContacts healing;
        combat::ProjectileContact contact;
        float contactLift = 0;
        uint32_t target = 0;
        float range = 0;
        std::array<float, 3> facing{};
    };
    std::array<Shot, 8> shots_{};
    uint32_t activeTarget_ = 0;
    bool activeMoving_ = false;
    std::array<float, 3> activeFacing_{{1, 0, 0}};
    SkillExecution execution_;
    const SkillCandidate* active_ = nullptr;
    std::map<uint8_t, uint32_t> previous_;
    std::map<uint32_t, uint64_t> ready_;
    std::map<uint32_t, uint64_t> deadlines_;
    std::map<uint32_t, uint32_t> resume_;
    bool restored_ = false;
    bool initiativeProfession_ = false, initiativeAnnounced_ = false;
    bool elementalProfession_ = false, attunementReported_ = false;
    uint8_t attunement_ = 1;
    unsigned unlockLevel_ = 1;
    ThiefStance stance_;
    beta::Conditions conditions_;
    beta::PhoenixReturn phoenix_;
    beta::Symbol2527Execution symbol_;
    uint16_t counterTarget_ = 0;
    std::string counterIdentity_;
    bool protectionCancelled_ = false, rocksCancelled_ = false;
    uint32_t lastInstantIdentity_ = 0;
    unsigned level_ = 1;
};
} // namespace june14505
} // namespace glide
