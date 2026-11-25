#pragma once
#include "glide/profile/game_messages.h"
#include <functional>
#include <string>
#include <vector>
#include <memory>

namespace glide {
struct WorldConditionActor {
    uint16_t agent = 0;
    std::string identity;
    CatalogPosition position;
    std::string conditions;
    std::string health;
};
struct WorldConditionChange {
    uint16_t agent = 0;
    std::string identity, before, after;
};
struct WorldHostileTarget {
    uint16_t agent = 0;
    std::string identity;
};
struct WorldHealthChange {
    uint16_t agent = 0;
    std::string identity, before, after;
    bool blocked = false;
    bool evaded = false;
    uint64_t impactUtc = 0;
    bool missed = false; // Source blindness: no damage or on-hit benefits.
};
struct WorldCombatTarget {
    WorldConditionActor actor;
    unsigned level = 0;
    std::string positiveEffects;
    bool blocking = false;
    bool evading = false;
};
enum class WorldConditionCommit { NotApplied, Applied, RecoveryRequired };
struct WorldReceivedEffectTarget {
    uint16_t agent = 0;
    std::string identity;
    unsigned definition = 0, refreshInstance = 0;
    uint64_t endsUtc = 0;
};
struct WorldActionContext {
    uint64_t now = 0;
    uint64_t wallNow = 0; // UTC milliseconds for deadlines that survive a server restart.
    uint32_t sequence = 0;
    uint16_t peerAgent = 0;
    unsigned combatLevel = 0;   // Authenticated effective level, never client input.
    unsigned combatTrait = 0;   // Explicit saved choice; no automatic trait activation.
    std::string selfIdentity;   // Character GUID plus current world-session owner.
    uint16_t observerAgent = 0; // Recipient of a late peer snapshot; zero for ordinary actions.
    CatalogPosition position;
    Facing facing;
    bool hasFacing = false;
    // Reserve the exact encoded sequence range before the first storage call.
    // A failed save abandons that range; another actor may already use the next.
    std::function<bool(uint32_t)> reserveSequences;
    bool ReserveSequences(uint32_t count) const {
        return !reserveSequences || reserveSequences(count);
    }
    // Valid only during the current request under the server state lock.
    // Controllers must prepare replies before calling and retain no callback.
    // Storage may release/reacquire the lock; publish controller state afterwards.
    std::function<bool(const std::string&, const std::string&)> persistInventory;
    std::function<bool(const std::string&)> persistSkillRecharges;
    std::function<bool(uint8_t)> persistProfessionState;
    // Equipped base legends and swap deadline, committed together.
    std::function<bool(const std::string&)> persistLegendState;
    // Switching legends and ending maintained skills must use one account write.
    std::function<bool(const std::string&, const std::string&, const std::string&)> persistLegendSkillState;
    // Cancel future effects and select the new profession state in one write.
    std::function<bool(uint8_t, const std::string&, const std::string&)> persistProfessionSkillState;
    // Save both fields in one account replacement before publishing an action.
    std::function<bool(const std::string&, const std::string&)> persistSkillState;
    bool stunned = false, taunted = false, revealed = false;
    uint64_t controlImpactUtc = 0; // Nominal hostile event, independent of delayed persistence.
    // Actual accepted29209 only: energy/recharge and release of control together.
    std::function<bool(const std::string&, const std::string&)> persistStunBreakState;
    std::string healthState; // Current durable state, copied under the server lock.
    std::function<bool(const std::string&, const std::string&, const std::string&)> persistCombatState;
    std::string conditionState;
    // Skill 26937 activation: health/resources and six expiring charges in one write.
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&,
                       const std::string&, uint64_t)>
        persistAssassinActivation;
    std::string removedBoons;
    std::string skillEffects; // Exact durable text, including any maintained-action envelope.
    // Conditions, positive effects, health and recharge must commit together.
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&)>
        persistCombatConditions;
    std::string enduranceState;
    std::string initiativeState;
    // HoT resource, conditions, health and skill state commit in one replacement.
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&,
                       const std::string&)>
        persistCombatEndurance;
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&,
                       const std::string&)>
        persistCombatInitiative;
    // A thief's endurance and initiative may refill in the same Tick.
    std::function<bool(const std::string&, const std::string&, const std::string&, const std::string&,
                       const std::string&, const std::string&)>
        persistCombatResources;
    // Same-map eligible allies, pinned for this request. The action selects its
    // own range/count from this snapshot; actor IDs alone are not identities.
    std::function<bool(std::vector<WorldConditionActor>&)> conditionAllies;
    // Health-only arrival: all recipients commit together before publication.
    std::function<WorldConditionCommit(const std::string&, const std::vector<WorldHealthChange>&)> persistAlliedHealth;
    // Same transaction with an explicit skill policy; the server authenticates
    // the caster level and validates that skill's amount and recipient count.
    std::function<WorldConditionCommit(unsigned, const std::string&, const std::vector<WorldHealthChange>&)>
        persistAlliedSkillHealth;
    // Own combat state plus donor changes form one durable operation. A durable
    // commit awaiting recovery must stop the session, never retry the action.
    // Donor controllers reconcile their condition caches on the next Tick.
    std::function<WorldConditionCommit(const std::string&, const std::string&, const std::string&, const std::string&,
                                       const std::vector<WorldConditionChange>&)>
        persistConditionTransfer;
    // The caster prepares its own effect copy. The server prepares the other
    // controllers and commits all durable states together, returning their IDs.
    std::function<WorldConditionCommit(const std::string&, const std::string&,
                                       const std::vector<WorldReceivedEffectTarget>&, std::vector<unsigned>&)>
        persistAlliedEffects;
    // Pure hostile effects: caster consumption and recipients commit without a health hit.
    std::function<WorldConditionCommit(unsigned, uint64_t, const std::string&, const std::string&,
                                       const std::vector<WorldHostileTarget>&)>
        persistHostileEffects;
    // Recipient IDs are allocated on prepared copies before this callback
    // serializes the caster's contribution map. All texts commit together.
    // The callback must change only a disposable action candidate, never live state.
    std::function<WorldConditionCommit(const std::string&,
                                       const std::function<bool(const std::vector<unsigned>&, std::string&)>&,
                                       const std::vector<WorldReceivedEffectTarget>&, std::vector<unsigned>&)>
        persistPreparedAlliedEffects;
    // Under the state lock: another announced character in this same map.
    // Do not retain the callback or assume a target remains after this request.
    std::function<bool(uint32_t)> isVisibleCharacterTarget;
    // Same visibility and lifetime checks, plus a copy of the current position.
    // Physics coordinates, matching context.position. Never retain the callback.
    std::function<bool(uint32_t, CatalogPosition&)> characterTargetPosition;
    // Current live enemy only: both players explicitly opted in, same map and
    // pinned world-session identity. Never infer hostility from visibility.
    std::function<bool(uint32_t, WorldCombatTarget&)> combatTarget;
    std::function<bool(std::vector<WorldCombatTarget>&)> combatEnemies;
    // All enemies of one area strike commit together before publication.
    std::function<WorldConditionCommit(unsigned, const std::string&, const std::vector<WorldHealthChange>&)>
        persistCombatHits;
    // Current hostile target health, compare-and-save before publishing a hit.
    std::function<WorldConditionCommit(unsigned, const std::string&, const WorldHealthChange&)> persistCombatHit;
    // One enemy hit and its allied area healing form the same durable operation.
    std::function<WorldConditionCommit(unsigned, const std::string&, const WorldHealthChange&,
                                       const std::vector<WorldHealthChange>&)>
        persistCombatHitWithAllies;
};
struct WorldActionOutput {
    Buffer owned, peers;
    // The target sees its own native actor, rather than its remote reservation.
    // Other recipients still receive peers. Both fields must be set together.
    Buffer targetView;
    uint16_t targetPeerAgent = 0;
    uint32_t sequences = 2;                 // Legacy controllers reserve two; new batches report their actual use.
    FlightEvent flight = FlightEvent::None; // Controller-reported deployment or stop, independent of wire format.
    uint64_t peerEffectVersion = 0;         // This batch already contains the subject's effect changes.
    // Position synchronization already encoded in owned/peers, in this map.
    // Commit the same position to the server so later targeting/arrivals agree.
    bool relocate = false;
    CatalogPosition destination;
};
enum class WorldActionResult { Ignored, Handled, Failed };
struct ReceivedWorldEffect {
    unsigned definition = 0;
    unsigned refreshInstance = 0; // Zero allocates; otherwise refresh only this contribution.
    uint64_t endsUtc = 0;
    unsigned sourceLevel = 0; // 718: trusted emitter level; zero means legacy/unknown.
};
// Prepared under the server state lock. Discarding this object changes nothing.
// The server must commit its durable text before calling Commit exactly once,
// without another action on this controller between preparation and commit.
class PreparedWorldEffect {
  public:
    virtual ~PreparedWorldEffect() {}
    std::string before, after;
    unsigned instance = 0;
    // One ID per requested contribution, in request order. A receiver prepares
    // all contributions on one copy so a durable batch cannot overwrite itself.
    std::vector<unsigned> instances;
    WorldActionOutput output;
    virtual void Commit() noexcept = 0;
};
class PreparedHostileEffects {
  public:
    bool controlBlocked = false;
    virtual ~PreparedHostileEffects() {}
    std::string beforeEffects, afterEffects, beforeConditions, afterConditions, beforeRemoved, afterRemoved;
    WorldActionOutput output;
    virtual void Commit() noexcept = 0;
};
// One instance per world connection; server calls under its state lock.
// Each request/tick executes once, then publishes its two encoded views.
// Destruction on leave/transfer discards deadlines and references to that world.
class PreparedWorldControl {
  public:
    WorldActionOutput output;
    std::function<void()> commit;
    void Commit() noexcept {
        commit();
    }
};
class WorldActions {
  public:
    virtual ~WorldActions() {}
    // Called only under the server state lock, like requests and received effects.
    virtual bool BlocksIncoming(uint64_t) const {
        return false;
    }
    virtual bool EvadesIncoming(uint64_t) const {
        return false;
    }
    virtual bool BlockIncoming(uint16_t, const std::string&, uint64_t) {
        return false;
    }
    virtual WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) = 0;
    virtual WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) = 0;
    // Select the current weapon's basic action. The server submits this through
    // Request so normal timing, resources, animation and durable impact apply.
    virtual WorldActionResult BasicAttackRequest(const WorldActionContext&, uint16_t, uint32_t, Buffer&) const {
        return WorldActionResult::Ignored;
    }
    // Complete a due durable impact before accepting a new request. The server
    // then rebuilds the context, including health and sequence reservations.
    virtual bool RequiresRequestTick(ByteView, uint64_t) const {
        return false;
    }
    virtual WorldActionResult PrepareStun(const WorldActionContext&, std::unique_ptr<PreparedWorldControl>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareDemonLeapImpact(const WorldActionContext&, uint64_t,
                                                     std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareElementalBlast(const WorldActionContext&, uint64_t,
                                                    std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareBurstStrength(const WorldActionContext&, uint64_t,
                                                   std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareGazeDarkness(const WorldActionContext&, uint64_t,
                                                  std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareEngagement(const WorldActionContext&, uint64_t,
                                                std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareStability(const WorldActionContext&, uint64_t,
                                               std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareBanish(const WorldActionContext&, uint64_t,
                                            std::unique_ptr<PreparedHostileEffects>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareReceivedEffect(const WorldActionContext&, const ReceivedWorldEffect&,
                                                    std::unique_ptr<PreparedWorldEffect>&) {
        return WorldActionResult::Ignored;
    }
    virtual WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                                     const std::vector<ReceivedWorldEffect>& requests,
                                                     std::unique_ptr<PreparedWorldEffect>& prepared) {
        if (requests.size() != 1)
            return WorldActionResult::Ignored;
        const auto result = PrepareReceivedEffect(context, requests.front(), prepared);
        if (result == WorldActionResult::Handled && prepared)
            prepared->instances = {prepared->instance};
        return result;
    }
    virtual bool PeerEffectsReady(uint64_t) const {
        return true;
    }
    virtual uint64_t PeerEffectVersion() const {
        return 0;
    }
    virtual WorldActionResult PeerEffect(const WorldActionContext&, bool, Buffer&) const {
        return WorldActionResult::Ignored;
    }
    // Append transient actors that must disappear before the owner leaves.
    virtual bool PeerCleanup(uint32_t, Buffer&) const {
        return true;
    }
};
} // namespace glide
