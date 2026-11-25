#include "glide/profile/condition_damage.h"
#include "glide/profile/reveal_state.h"
#include "glide/profile/forced_engagement.h"
#include "glide/profile/jade_winds.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/boon_removal.h"
#include "glide/profile/assassin_siphon.h"
#include "glide/profile/initiative.h"
#include <cmath>
#include "server.h"
#include "glide/crypto.h"
#include "glide/guid.h"
#include "glide/srp.h"
#include "glide/profile/modern_health.h"
#include "glide/profile/june_health.h"
#include "glide/profile/beta_melee_combat.h"
#include <Windows.h>
#include <cstring>

namespace glide {
namespace {
std::string ConditionPacketTail(const Buffer& bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string text;
    const size_t start = bytes.Size() > 96 ? bytes.Size() - 96 : 0;
    for (size_t i = start; i < bytes.Size(); ++i) {
        const auto byte = bytes.Data()[i];
        text += digits[byte >> 4];
        text += digits[byte & 15];
    }
    return text;
}
std::shared_ptr<const Buffer> PeerMessage(Buffer&& bytes, const std::shared_ptr<const PeerIdentity>& identity,
                                          const std::shared_ptr<const PeerIdentity>& first = {},
                                          const std::shared_ptr<const PeerIdentity>& second = {}) {
    struct Pending {
        Buffer bytes;
        std::shared_ptr<const PeerIdentity> identity, first, second;
    };
    auto pending = std::make_shared<Pending>();
    pending->bytes = std::move(bytes);
    pending->identity = identity;
    pending->first = first;
    pending->second = second;
    // A queued or currently sending message retains its identity reservation.
    return std::shared_ptr<const Buffer>(pending, &pending->bytes);
}
} // namespace
void Server::QueuePeerLocked(SessionState& recipient, const std::shared_ptr<const Buffer>& message) {
    if (recipient.outgoingOverflow)
        return;
    if (recipient.outgoing.size() >= 256 || message->Size() > 256 * 1024 - recipient.outgoingBytes) {
        recipient.outgoingOverflow = true;
        recipient.outgoing.clear();
        recipient.outgoingBytes = 0;
        return;
    }
    recipient.outgoing.push_back(message);
    recipient.outgoingBytes += message->Size();
}
bool Server::JoinPeersLocked(SessionState& state) {
    if (!profile_ || !state.activeWorld || !state.peerIdentity || state.peerAnnounced ||
        nextSequence_ > UINT32_MAX - 10)
        return false;
    const unsigned map = state.activeWorld->character.map;
    const auto identity = state.peerIdentity;
    const uint32_t sequence = nextSequence_;
    nextSequence_ += 10;
    auto appearance = [&](const SessionState& subject, const std::shared_ptr<const PeerIdentity>& id) {
        BootstrapContext context{*subject.activeWorld, catalog_};
        context.sequence = sequence;
        context.identity = id.get();
        std::vector<BootstrapStep> steps;
        Buffer bytes;
        if (!Bootstrap().PeerSteps(context, *id, steps))
            return std::shared_ptr<const Buffer>();
        for (const auto& step : steps)
            if (step.kind != BootstrapStep::Kind::Send || !bytes.Append(step.payload.View()))
                return std::shared_ptr<const Buffer>();
        if (subject.mountSkin) {
            Buffer mount;
            if (!subject.gameMessages->Mount(subject.mountContent, subject.mountDefinition, id->character, mount) ||
                !bytes.Append(mount.View()))
                return std::shared_ptr<const Buffer>();
        }
        // A new world connection always starts outside combat. Existing duelists
        // must also appear friendly to that newcomer.
        Buffer attitude;
        if (!subject.gameMessages->PeerAttitude(id->character, false, attitude) || !bytes.Append(attitude.View()))
            return std::shared_ptr<const Buffer>();
        return PeerMessage(std::move(bytes), id);
    };
    const auto own = appearance(state, identity);
    if (!own)
        return false;
    std::vector<std::pair<SessionState*, std::shared_ptr<const Buffer>>> others;
    for (auto& pair : sessions_) {
        auto& peer = pair.second;
        if (&peer == &state || peer.loggedOut || !peer.worldReady || peer.outgoingOverflow || !peer.activeWorld ||
            !peer.peerIdentity || peer.activeWorld->character.map != map)
            continue;
        auto message = appearance(peer, peer.peerIdentity);
        if (!message)
            return false;
        others.emplace_back(&peer, std::move(message));
    }
    state.peerAnnounced = true;
    for (const auto& pair : others) {
        QueuePeerLocked(state, pair.second);
        QueuePeerLocked(*pair.first, own);
    }
    // Queue the initial effect state under the same lock as appearance. A later
    // broadcast must not mark a newcomer current after sending only one of
    // several already active projectiles/buffs. Profiles awaiting restoration
    // still defer through PeerEffectsReady and the normal polling path.
    return !state.outgoingOverflow && QueuePeerEffectsLocked(state);
}
bool Server::CombatModeLocked(SessionState& state, bool enabled) {
    if (state.combatEnabled == enabled)
        return true;
    // Finish any in-flight save involving this character before changing who
    // may damage or heal it. The guard restores target availability on exit.
    WorldTargetExit targetExit(*this, state.worldTargets);
    if (!state.peerAnnounced || !state.activeWorld || !state.peerIdentity || !state.gameMessages ||
        state.outgoingOverflow)
        return false;
    std::vector<std::pair<SessionState*, std::shared_ptr<const Buffer>>> updates;
    for (auto& pair : sessions_) {
        auto& peer = pair.second;
        if (&peer == &state || peer.loggedOut || !peer.worldReady || !peer.peerAnnounced || peer.outgoingOverflow ||
            !peer.activeWorld || !peer.peerIdentity || !peer.gameMessages || !peer.combatEnabled ||
            peer.activeWorld->character.map != state.activeWorld->character.map)
            continue;
        Buffer own, remote;
        if (!state.gameMessages->PeerAttitude(peer.peerIdentity->character, enabled, own) ||
            !peer.gameMessages->PeerAttitude(state.peerIdentity->character, enabled, remote))
            return false;
        updates.emplace_back(&state, PeerMessage(std::move(own), peer.peerIdentity));
        updates.emplace_back(&peer, PeerMessage(std::move(remote), state.peerIdentity));
    }
    // Prepare both views before publishing any change. Observers outside combat
    // keep the friendly relation sent with each character's appearance.
    state.combatEnabled = enabled;
    for (const auto& update : updates)
        QueuePeerLocked(*update.first, update.second);
    return true;
}
void Server::LeavePeersLocked(SessionState& state) {
    WaitWorldTargetsLocked(state.worldTargets);
    state.worldTargets.reset();
    state.combatEnabled = false;
    if (state.peerIdentity)
        for (auto& pair : sessions_)
            if (pair.second.tauntCaster == state.peerIdentity && !RefreshTauntLocked(pair.second))
                pair.second.outgoingOverflow = true;
    state.peerRevealVersions.clear();
    state.publishedReveal.clear();
    state.revealVersion = 0;
    for (auto& pair : sessions_)
        pair.second.peerRevealVersions.erase(state.worldOwner);
    state.peerTauntVersions.clear();
    state.publishedTaunt.clear();
    state.tauntCaster.reset();
    state.tauntVersion = 0;
    for (auto& pair : sessions_)
        pair.second.peerTauntVersions.erase(state.worldOwner);
    state.peerEffectVersions.clear();
    for (auto& pair : sessions_)
        pair.second.peerEffectVersions.erase(state.worldOwner);

    state.requestedMap = 0;
    state.hasFacing = false;
    state.facing = {};
    state.controlMotion.Clear();
    state.controlCorrectionAt = 0;
    state.controlCorrectionSent = false;
    state.flying = false;
    state.flightMotion.Clear();
    state.mountSkin = 0;
    state.mountDefinition = 0;
    state.mountContent = 0;
    if (!state.peerIdentity || !state.activeWorld || !state.peerAnnounced) {
        state.peerIdentity.reset();
        state.peerAnnounced = false;
        return;
    }
    Buffer bytes;
    const bool encoded = nextSequence_ && nextSequence_ < UINT32_MAX &&
                         (!state.worldActions || state.worldActions->PeerCleanup(nextSequence_, bytes)) && [&] {
                             Buffer removal;
                             return Bootstrap().PeerRemove(*state.peerIdentity, nextSequence_++, removal) &&
                                    bytes.Append(removal.View());
                         }();
    const auto message = encoded ? PeerMessage(std::move(bytes), state.peerIdentity) : std::shared_ptr<const Buffer>();
    for (auto& pair : sessions_) {
        auto& peer = pair.second;
        if (&peer == &state || peer.loggedOut || !peer.worldReady || !peer.activeWorld ||
            peer.activeWorld->character.map != state.activeWorld->character.map)
            continue;
        if (message)
            QueuePeerLocked(peer, message);
        else {
            peer.outgoingOverflow = true;
            peer.outgoing.clear();
            peer.outgoingBytes = 0;
        }
    }
    state.peerIdentity.reset();
    state.peerAnnounced = false;
}
bool Server::UpdateWorldPosition(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                 const CatalogPosition& position, CatalogPosition* authoritative) {
    if (!session || !owner || !std::isfinite(position.x * 32) || !std::isfinite(position.y * 32) ||
        !std::isfinite(position.z * 32))
        return false;
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld)
        return false;
    auto& character = at->second.activeWorld->character;
    combat::StunState control;
    if (!control.Load(profile_->Info().build, combat::StunState::Field(character)))
        return false;
    if (control.Active(wallClock_())) {
        if (authoritative)
            *authoritative = {character.x, character.y, character.z};
        return true;
    }
    character.x = position.x;
    character.y = position.y;
    character.z = position.z;
    if (authoritative)
        *authoritative = {character.x, character.y, character.z};
    return true;
}
void Server::BroadcastPeerLocked(SessionState& subject, Buffer&& bytes, uint16_t targetAgent, Buffer&& targetBytes,
                                 uint64_t effectsVersion, const std::shared_ptr<const PeerIdentity>& extraIdentity) {
    const auto message = PeerMessage(std::move(bytes), subject.peerIdentity, extraIdentity);
    const auto target =
        targetBytes.Size() ? PeerMessage(std::move(targetBytes), subject.peerIdentity, extraIdentity) : nullptr;
    for (auto& pair : sessions_) {
        auto& peer = pair.second;
        if (&peer != &subject && !peer.loggedOut && peer.worldReady && peer.peerIdentity && peer.activeWorld &&
            peer.activeWorld->character.map == subject.activeWorld->character.map) {
            QueuePeerLocked(peer, target && peer.peerIdentity->character == targetAgent ? target : message);
            // A delivered creation must not also be queued as a first snapshot.
            // Original clients require each active buff instance to be unique.
            if (effectsVersion)
                peer.peerEffectVersions[subject.worldOwner] = effectsVersion;
        }
    }
}
Server::SessionState* Server::TauntSourceLocked(const SessionState& victim, const std::string& identity) {
    if (!victim.combatEnabled || !victim.activeWorld)
        return nullptr;
    for (auto& pair : sessions_) {
        auto& source = pair.second;
        if (&source != &victim && !source.loggedOut && source.worldReady && source.peerAnnounced &&
            source.combatEnabled && source.activeWorld && source.peerIdentity &&
            source.activeWorld->character.map == victim.activeWorld->character.map &&
            source.activeWorld->character.guid.Text() + ":" + std::to_string(source.worldOwner) == identity)
            return &source;
    }
    return nullptr;
}
bool Server::RefreshRevealLocked(SessionState& subject) {
    if (!subject.activeWorld || !subject.peerIdentity)
        return true;
    const auto build = profile_->Info().build;
    const auto utc = wallClock_();
    combat::RevealState next;
    if (!next.Load(build, combat::RevealState::Field(subject.activeWorld->character)))
        return false;
    const auto text = subject.worldReady && subject.peerAnnounced && next.Active(utc) ? next.Text() : std::string();
    if (subject.publishedReveal == text)
        return true;
    if (subject.revealVersion == UINT64_MAX)
        return false;
    const bool remove = !subject.publishedReveal.empty();
    std::vector<std::pair<SessionState*, std::shared_ptr<const Buffer>>> queued;
    for (auto& pair : sessions_) {
        auto& recipient = pair.second;
        if (recipient.loggedOut || !recipient.worldReady || !recipient.peerAnnounced || !recipient.activeWorld ||
            !recipient.peerIdentity || recipient.activeWorld->character.map != subject.activeWorld->character.map)
            continue;
        const auto actor = &recipient == &subject ? beta::CombatLocalActor(build, subject.peerIdentity->character)
                                                  : subject.peerIdentity->character;
        Buffer packet;
        if (remove && !combat::RevealState::Remove(build, actor, packet))
            return false;
        if (!text.empty() &&
            !next.Create(build, actor, utc, beta::CombatLevel(build, subject.activeWorld->character), packet))
            return false;
        if (packet.Size())
            queued.emplace_back(&recipient, PeerMessage(std::move(packet), subject.peerIdentity));
    }
    subject.publishedReveal = text;
    ++subject.revealVersion;
    for (auto& item : queued) {
        QueuePeerLocked(*item.first, item.second);
        item.first->peerRevealVersions[subject.worldOwner] = subject.revealVersion;
        if (item.first->outgoingOverflow)
            return false;
    }
    return true;
}
bool Server::QueueRevealSnapshotLocked(SessionState& recipient, SessionState& subject) {
    if (!subject.revealVersion)
        return true;
    const auto seen = recipient.peerRevealVersions.find(subject.worldOwner);
    if (seen != recipient.peerRevealVersions.end() && seen->second == subject.revealVersion)
        return true;
    if (!subject.publishedReveal.empty()) {
        combat::RevealState reveal;
        const auto build = profile_->Info().build;
        const auto utc = wallClock_();
        if (!reveal.Load(build, subject.publishedReveal))
            return false;
        if (!reveal.Active(utc))
            return RefreshRevealLocked(subject);
        const auto actor = &recipient == &subject ? beta::CombatLocalActor(build, subject.peerIdentity->character)
                                                  : subject.peerIdentity->character;
        Buffer packet;
        if (!reveal.Create(build, actor, utc, beta::CombatLevel(build, subject.activeWorld->character), packet))
            return false;
        QueuePeerLocked(recipient, PeerMessage(std::move(packet), subject.peerIdentity));
        if (recipient.outgoingOverflow)
            return false;
    }
    recipient.peerRevealVersions[subject.worldOwner] = subject.revealVersion;
    return true;
}
bool Server::RefreshTauntLocked(SessionState& subject, bool force) {
    if (!subject.activeWorld || !subject.peerIdentity)
        return true;
    const auto build = profile_->Info().build;
    const auto utc = wallClock_();
    combat::TauntState next;
    if (!next.Load(build, combat::TauntState::Field(subject.activeWorld->character)))
        return false;
    auto* source = next.Active(utc) && subject.worldReady && subject.peerAnnounced
                       ? TauntSourceLocked(subject, next.SourceIdentity())
                       : nullptr;
    const auto caster = source ? source->peerIdentity : std::shared_ptr<const PeerIdentity>();
    const auto text = caster ? next.Text() : std::string();
    if (!force && subject.publishedTaunt == text && subject.tauntCaster == caster)
        return true;
    if (subject.tauntVersion == UINT64_MAX)
        return false;
    const auto old = subject.tauntCaster;
    const bool remove = !subject.publishedTaunt.empty() && old;
    std::vector<std::pair<SessionState*, std::shared_ptr<const Buffer>>> queued;
    for (auto& pair : sessions_) {
        auto& recipient = pair.second;
        if (recipient.loggedOut || !recipient.worldReady || !recipient.peerAnnounced || !recipient.activeWorld ||
            !recipient.peerIdentity || recipient.activeWorld->character.map != subject.activeWorld->character.map)
            continue;
        const auto actor = &recipient == &subject ? beta::CombatLocalActor(build, subject.peerIdentity->character)
                                                  : subject.peerIdentity->character;
        Buffer packet;
        if (remove) {
            const auto prior =
                recipient.peerIdentity == old ? beta::CombatLocalActor(build, old->character) : old->character;
            if (!combat::TauntState::Remove(build, actor, prior, packet))
                return false;
        }
        if (caster) {
            const auto owner =
                recipient.peerIdentity == caster ? beta::CombatLocalActor(build, caster->character) : caster->character;
            if (!next.Create(build, actor, owner, utc, beta::CombatLevel(build, source->activeWorld->character),
                             packet))
                return false;
        }
        if (packet.Size())
            queued.emplace_back(&recipient, PeerMessage(std::move(packet), subject.peerIdentity, old, caster));
    }
    subject.publishedTaunt = text;
    subject.tauntCaster = caster;
    ++subject.tauntVersion;
    for (auto& item : queued) {
        QueuePeerLocked(*item.first, item.second);
        item.first->peerTauntVersions[subject.worldOwner] = subject.tauntVersion;
        if (item.first->outgoingOverflow)
            return false;
    }
    return true;
}
bool Server::QueueTauntSnapshotLocked(SessionState& recipient, SessionState& subject) {
    if (!subject.tauntVersion)
        return true;
    const auto seen = recipient.peerTauntVersions.find(subject.worldOwner);
    if (seen != recipient.peerTauntVersions.end() && seen->second == subject.tauntVersion)
        return true;
    if (!subject.publishedTaunt.empty() && subject.tauntCaster) {
        combat::TauntState taunt;
        if (!taunt.Load(profile_->Info().build, subject.publishedTaunt))
            return false;
        auto* source = TauntSourceLocked(subject, taunt.SourceIdentity());
        if (!source || source->peerIdentity != subject.tauntCaster)
            return false;
        const auto actor = recipient.peerIdentity == subject.peerIdentity
                               ? beta::CombatLocalActor(profile_->Info().build, subject.peerIdentity->character)
                               : subject.peerIdentity->character;
        const auto caster = recipient.peerIdentity == source->peerIdentity
                                ? beta::CombatLocalActor(profile_->Info().build, source->peerIdentity->character)
                                : source->peerIdentity->character;
        const auto utc = wallClock_();
        if (!taunt.Active(utc))
            return RefreshTauntLocked(subject);
        Buffer packet;
        if (!taunt.Create(profile_->Info().build, actor, caster, utc,
                          beta::CombatLevel(profile_->Info().build, source->activeWorld->character), packet))
            return false;
        QueuePeerLocked(recipient, PeerMessage(std::move(packet), subject.peerIdentity, source->peerIdentity));
        if (recipient.outgoingOverflow)
            return false;
    }
    recipient.peerTauntVersions[subject.worldOwner] = subject.tauntVersion;
    return true;
}
bool Server::QueuePeerEffectsLocked(SessionState& recipient) {
    const uint64_t now = clock_();
    if (!recipient.activeWorld || !recipient.peerAnnounced)
        return true;
    if (recipient.worldActions && !recipient.worldActions->PeerEffectsReady(now))
        return true;
    for (auto& pair : sessions_) {
        auto& subject = pair.second;
        if (&subject == &recipient || subject.loggedOut || !subject.worldReady || !subject.peerAnnounced ||
            !subject.activeWorld || !subject.peerIdentity || !subject.worldActions ||
            subject.activeWorld->character.map != recipient.activeWorld->character.map)
            continue;
        if (!RefreshTauntLocked(subject) || !QueueTauntSnapshotLocked(recipient, subject))
            return false;
        if (!RefreshRevealLocked(subject) || !QueueRevealSnapshotLocked(recipient, subject))
            return false;
        const uint64_t version = subject.worldActions->PeerEffectVersion();
        if (!version)
            continue;
        const auto seen = recipient.peerEffectVersions.find(subject.worldOwner);
        if (seen != recipient.peerEffectVersions.end() && seen->second == version)
            continue;
        if (nextSequence_ == UINT32_MAX)
            return false;
        WorldActionContext context;
        context.now = now;
        context.wallNow = wallClock_();
        context.sequence = nextSequence_;
        context.peerAgent = subject.peerIdentity->character;
        const auto& character = subject.activeWorld->character;
        context.position = CatalogPosition{character.x, character.y, character.z};
        context.facing = subject.facing;
        context.hasFacing = subject.hasFacing;
        context.observerAgent = recipient.peerIdentity ? recipient.peerIdentity->character : 0;
        Buffer effect;
        const auto result =
            subject.worldActions->PeerEffect(context, seen != recipient.peerEffectVersions.end(), effect);
        if (result == WorldActionResult::Failed || (result == WorldActionResult::Ignored && effect.Size()))
            return false;
        if (effect.Size()) {
            QueuePeerLocked(recipient, PeerMessage(std::move(effect), subject.peerIdentity));
            ++nextSequence_;
            if (recipient.outgoingOverflow)
                return false;
        }
        recipient.peerEffectVersions[subject.worldOwner] = version;
    }
    return true;
}
WorldActionResult Server::RunWorldActionsLocked(SessionState& state, ByteView input, bool tick, bool forcedAttack) {
    if (!state.worldActions)
        return WorldActionResult::Ignored;
    if (!state.worldReady || !state.activeWorld || !state.peerIdentity || state.outgoingOverflow ||
        nextSequence_ > UINT32_MAX - 2)
        return WorldActionResult::Failed;
    if (forcedAttack && state.conditionDamagePending)
        return WorldActionResult::Handled;
    if (!forcedAttack) {
        const auto damage = TickConditionDamageLocked(state, wallClock_());
        if (damage != WorldActionResult::Ignored)
            return damage;
    }
    combat::StunState control;
    if (!control.Load(profile_->Info().build, combat::StunState::Field(state.activeWorld->character)))
        return WorldActionResult::Failed;
    combat::TauntState taunt;
    if (!taunt.Load(profile_->Info().build, combat::TauntState::Field(state.activeWorld->character)))
        return WorldActionResult::Failed;
    auto* source = taunt.Active(wallClock_()) ? TauntSourceLocked(state, taunt.SourceIdentity()) : nullptr;
    const auto forcedSource = forcedAttack && source ? source->peerIdentity : std::shared_ptr<const PeerIdentity>();
    if (forcedAttack && (!forcedSource || control.Active(wallClock_())))
        return WorldActionResult::Ignored;
    if (!forcedAttack && !tick && source &&
        combat::ForcedEngagement::RestrictedRequest(profile_->Info().build, input) &&
        !combat::StunBreakRequest(profile_->Info().build, state.peerIdentity->character, input))
        return WorldActionResult::Handled;
    if (!tick && control.Active(wallClock_()) &&
        !combat::StunBreakRequest(profile_->Info().build, state.peerIdentity->character, input))
        return WorldActionResult::Handled;
    auto actionNow = clock_();
    if (!tick && !forcedAttack && state.worldActions->RequiresRequestTick(input, actionNow)) {
        if (RunWorldActionsLocked(state, ByteView(), true) == WorldActionResult::Failed)
            return WorldActionResult::Failed;
        if (!state.worldActions || !state.worldReady || !state.activeWorld || !state.peerIdentity || state.loggedOut ||
            state.outgoingOverflow || nextSequence_ > UINT32_MAX - 2)
            return WorldActionResult::Failed;
        actionNow = clock_();
        // A rejected write retains the old impact. Do not replace it with a
        // new order before its durable result is known.
        if (state.worldActions->RequiresRequestTick(input, actionNow))
            return WorldActionResult::Handled;
    }
    WorldTargetUses targets;
    WorldActionContext context;
    context.now = actionNow;
    context.sequence = nextSequence_;
    context.peerAgent = state.peerIdentity->character;
    context.wallNow = wallClock_();
    if (!control.Load(profile_->Info().build, combat::StunState::Field(state.activeWorld->character)))
        return WorldActionResult::Failed;
    context.stunned = control.Active(context.wallNow);
    combat::RevealState reveal;
    if (!reveal.Load(profile_->Info().build, combat::RevealState::Field(state.activeWorld->character)))
        return WorldActionResult::Failed;
    context.revealed = reveal.Active(context.wallNow);
    if (!RefreshRevealLocked(state))
        return WorldActionResult::Failed; // Expired890 must leave before a new stealth grant is published.
    if (!taunt.Load(profile_->Info().build, combat::TauntState::Field(state.activeWorld->character)))
        return WorldActionResult::Failed;
    context.taunted = taunt.Active(context.wallNow) && TauntSourceLocked(state, taunt.SourceIdentity());
    bool sequencesReserved = false;
    uint32_t reservedSequences = 0;
    context.reserveSequences = [&](uint32_t count) {
        if (sequencesReserved || count > 32 || context.sequence != nextSequence_ || count > UINT32_MAX - nextSequence_)
            return false;
        reservedSequences = count;
        sequencesReserved = true;
        nextSequence_ += count;
        return true;
    };
    const auto& character = state.activeWorld->character;
    context.combatLevel = beta::CombatLevel(profile_->Info().build, character);
    if (!combat::ReadCombatTrait(profile_->Info().build, character, context.combatTrait))
        return WorldActionResult::Failed;
    context.selfIdentity = character.guid.Text() + ":" + std::to_string(state.worldOwner);
    context.position = CatalogPosition{character.x, character.y, character.z};
    context.facing = state.facing;
    context.hasFacing = state.hasFacing;
    context.persistInventory = [&](const std::string& locations, const std::string& bindings) {
        std::string error;
        const bool saved = SaveWorldInventoryLocked(state, locations, bindings, error);
        if (!saved)
            QueueLogLocked("inventory", error);
        return saved;
    };
    context.persistSkillRecharges = [&](const std::string& deadlines) {
        std::string error;
        const bool saved = SaveWorldSkillRechargesLocked(state, deadlines, error, sequencesReserved);
        if (!saved)
            QueueLogLocked("skill-recharges", error);
        return saved;
    };
    context.persistProfessionState = [&](uint8_t selection) {
        std::string error;
        const bool saved = SaveWorldProfessionStateLocked(state, selection, error, sequencesReserved);
        if (!saved)
            QueueLogLocked("profession-state", error);
        return saved;
    };
    context.persistLegendState = [&](const std::string& legends) {
        std::string error;
        const bool saved = SaveWorldLegendStateLocked(state, legends, error, sequencesReserved);
        if (!saved)
            QueueLogLocked("legend-state", error);
        return saved;
    };
    context.persistLegendSkillState = [&](const std::string& legends, const std::string& deadlines,
                                          const std::string& effects) {
        std::string error;
        const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved, nullptr, 0,
                                                     nullptr, &legends);
        if (!saved)
            QueueLogLocked("legend-skill-state", error);
        return saved;
    };
    context.persistSkillState = [&](const std::string& deadlines, const std::string& effects) {
        std::string error;
        const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved);
        if (!saved)
            QueueLogLocked("skill-state", error);
        return saved;
    };
    context.persistStunBreakState = [&](const std::string& deadlines, const std::string& effects) {
        if (tick || (!context.stunned && !context.taunted) ||
            !combat::StunBreakRequest(profile_->Info().build, state.peerIdentity->character, input))
            return false;
        std::string error;
        const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved, nullptr, 0,
                                                     nullptr, nullptr, nullptr, nullptr, nullptr, true);
        if (!saved) {
            QueueLogLocked("stun-break", error);
            if (context.taunted && !RefreshTauntLocked(state, true))
                state.outgoingOverflow = true;
        } else if (!RefreshTauntLocked(state))
            state.outgoingOverflow = true;
        return saved;
    };
    context.persistProfessionSkillState = [&](uint8_t selection, const std::string& deadlines,
                                              const std::string& effects) {
        if (selection < 1 || selection > 4)
            return false;
        std::string error;
        const bool saved =
            SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved, nullptr, selection);
        if (!saved)
            QueueLogLocked("profession-skill-state", error);
        return saved;
    };
    const auto health = character.profileFields.find("health_state");
    context.healthState = health == character.profileFields.end() ? std::string() : health->second;
    const auto conditions = character.profileFields.find("condition_effects");
    context.conditionState = conditions == character.profileFields.end() ? std::string() : conditions->second;
    const auto endurance = character.profileFields.find("endurance_state");
    context.enduranceState = endurance == character.profileFields.end() ? std::string() : endurance->second;
    const auto profession = character.profileFields.find("profession");
    if ((profile_->Info().build == 49284 || profile_->Info().build == 167409 || profile_->Info().build == 204489) &&
        profession != character.profileFields.end() && profession->second.size() == 1 && profession->second[0] >= '1' &&
        profession->second[0] <= '9') {
        context.persistCombatEndurance = [&](const std::string& deadlines, const std::string& effects,
                                             const std::string& currentHealth, const std::string& currentConditions,
                                             const std::string& resource) {
            std::string error;
            const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved,
                                                         &currentHealth, 0, &currentConditions, nullptr, &resource);
            if (!saved)
                QueueLogLocked("endurance-state", error);
            return saved;
        };
    }
    const auto initiative = character.profileFields.find("initiative_state");
    context.initiativeState = initiative == character.profileFields.end() ? std::string() : initiative->second;
    if (modern::InitiativeBuild(profile_->Info().build) && profession != character.profileFields.end() &&
        profession->second == "5") {
        context.persistCombatInitiative =
            [&, expectedHealth = context.healthState, expectedConditions = context.conditionState](
                const std::string& deadlines, const std::string& effects, const std::string& currentHealth,
                const std::string& currentConditions, const std::string& resource) {
                std::string error;
                const bool saveHealth = profile_->SupportsHealthStatePersistence(),
                           saveConditions = profile_->SupportsConditionStatePersistence();
                if ((!saveHealth && currentHealth != expectedHealth) ||
                    (!saveConditions && currentConditions != expectedConditions))
                    return false;
                const bool saved = SaveWorldSkillStateLocked(
                    state, deadlines, effects, error, sequencesReserved, saveHealth ? &currentHealth : nullptr, 0,
                    saveConditions ? &currentConditions : nullptr, nullptr, nullptr, &resource);
                if (!saved)
                    QueueLogLocked("initiative-state", error);
                return saved;
            };
    }
    if ((profile_->Info().build == 49284 || profile_->Info().build == 167409 || profile_->Info().build == 204489) &&
        profession != character.profileFields.end() && profession->second == "5") {
        context.persistCombatResources = [&](const std::string& deadlines, const std::string& effects,
                                             const std::string& health, const std::string& conditions,
                                             const std::string& endurance, const std::string& initiative) {
            std::string error;
            const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved, &health,
                                                         0, &conditions, nullptr, &endurance, &initiative);
            if (!saved)
                QueueLogLocked("combat-resources", error);
            return saved;
        };
    }
    const auto skillEffects = character.profileFields.find("skill_effects");
    context.skillEffects = skillEffects == character.profileFields.end() ? std::string() : skillEffects->second;
    context.removedBoons = combat::BoonRemovals::Field(character);
    context.persistCombatConditions = [&](const std::string& deadlines, const std::string& effects,
                                          const std::string& currentHealth, const std::string& currentConditions) {
        std::string error;
        const bool saved = SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved,
                                                     &currentHealth, 0, &currentConditions);
        if (!saved)
            QueueLogLocked("combat-conditions", error);
        return saved;
    };
    if (combat::AssassinSiphon::Build(profile_->Info().build) && profession != character.profileFields.end() &&
        profession->second == "9")
        context.persistAssassinActivation = [&](const std::string& deadlines, const std::string& effects,
                                                const std::string& health, const std::string& conditions,
                                                const std::string& endurance, uint64_t activation) {
            combat::AssassinSiphon charges;
            std::string error;
            if (!sequencesReserved || activation > context.wallNow || !charges.Arm(activation))
                return false;
            const auto text = charges.Text();
            const bool saved =
                SaveWorldSkillStateLocked(state, deadlines, effects, error, true, &health, 0, &conditions, nullptr,
                                          endurance.empty() ? nullptr : &endurance, nullptr, &text);
            if (!saved)
                QueueLogLocked("assassin-siphon", error);
            return saved;
        };
    context.persistCombatState = [&](const std::string& deadlines, const std::string& effects,
                                     const std::string& currentHealth) {
        std::string error;
        const bool saved =
            SaveWorldSkillStateLocked(state, deadlines, effects, error, sequencesReserved, &currentHealth);
        if (!saved)
            QueueLogLocked("combat-state", error);
        return saved;
    };
    context.persistConditionTransfer = [&](const std::string& deadlines, const std::string& effects,
                                           const std::string& currentHealth, const std::string& currentConditions,
                                           const std::vector<WorldConditionChange>& donors) {
        if (!sequencesReserved)
            return WorldConditionCommit::NotApplied;
        std::string error;
        const auto result =
            SaveConditionTransferLocked(state, deadlines, effects, currentHealth, currentConditions, donors, error);
        if (result != WorldConditionCommit::Applied)
            QueueLogLocked("condition-transfer", error);
        return result;
    };
    context.persistHostileEffects = [&](unsigned skill, uint64_t nominal, const std::string& deadlines,
                                        const std::string& effects, const std::vector<WorldHostileTarget>& targets) {
        if (!sequencesReserved)
            return WorldConditionCommit::NotApplied;
        std::string error;
        const auto result =
            SaveHostileEffectsLocked(state, context, skill, nominal, deadlines, effects, targets, error);
        if (result != WorldConditionCommit::Applied)
            QueueLogLocked("hostile-effects", error);
        return result;
    };
    context.persistAlliedEffects = [&](const std::string& deadlines, const std::string& effects,
                                       const std::vector<WorldReceivedEffectTarget>& recipients,
                                       std::vector<unsigned>& instances) {
        if (!sequencesReserved)
            return WorldConditionCommit::NotApplied;
        std::string error;
        const auto result = SaveAlliedEffectsLocked(state, context, deadlines, effects, recipients, instances, error);
        if (result != WorldConditionCommit::Applied)
            QueueLogLocked("allied-effects", error);
        return result;
    };
    context.persistCombatHit = [&](unsigned skill, const std::string& ownHealth, const WorldHealthChange& target) {
        if (!sequencesReserved)
            return WorldConditionCommit::NotApplied;
        std::string error;
        const auto saved = SaveAlliedHealthLocked(state, context, ownHealth, {target}, error, skill, true);
        if (saved != WorldConditionCommit::Applied)
            QueueLogLocked("combat-health", error);
        return saved;
    };
    context.persistCombatHits = [&](unsigned skill, const std::string& ownHealth,
                                    const std::vector<WorldHealthChange>& enemies) {
        if (!sequencesReserved || enemies.empty() ||
            !((skill == 965 && beta::BetaHealthBuild(profile_->Info().build)) ||
              ((skill == combat::DemonLeapImpact::Skill || skill == combat::ElementalBlast::Skill ||
                skill == combat::BurstStrengthImpact::Skill || skill == combat::BanishEnchantment::Skill ||
                skill == combat::JadeWinds::Skill) &&
               combat::BanishEnchantment::Build(profile_->Info().build))))
            return WorldConditionCommit::NotApplied;
        std::string error;
        const auto saved = SaveAlliedHealthLocked(state, context, ownHealth, enemies, error, skill, true, true);
        if (saved != WorldConditionCommit::Applied)
            QueueLogLocked("combat-area", error);
        return saved;
    };
    context.persistCombatHitWithAllies = [&](unsigned skill, const std::string& ownHealth,
                                             const WorldHealthChange& target,
                                             const std::vector<WorldHealthChange>& allies) {
        if (!sequencesReserved || !beta::MeleeAreaHealing(profile_->Info().build, skill) ||
            allies.size() > beta::kMeleeHealingLimit)
            return WorldConditionCommit::NotApplied;
        std::vector<WorldHealthChange> changes{target};
        changes.insert(changes.end(), allies.begin(), allies.end());
        std::string error;
        const auto saved = SaveAlliedHealthLocked(state, context, ownHealth, changes, error, skill, true);
        if (saved != WorldConditionCommit::Applied)
            QueueLogLocked("combat-health", error);
        return saved;
    };
    if (beta::BetaHealthBuild(profile_->Info().build)) {
        context.persistAlliedHealth = [&](const std::string& health, const std::vector<WorldHealthChange>& recipients) {
            if (!sequencesReserved)
                return WorldConditionCommit::NotApplied;
            std::string error;
            const auto result = SaveAlliedHealthLocked(state, context, health, recipients, error);
            if (result != WorldConditionCommit::Applied)
                QueueLogLocked("allied-health", error);
            return result;
        };
        context.persistAlliedSkillHealth = [&](unsigned skill, const std::string& health,
                                               const std::vector<WorldHealthChange>& recipients) {
            if (!sequencesReserved)
                return WorldConditionCommit::NotApplied;
            std::string error;
            const auto result = SaveAlliedHealthLocked(state, context, health, recipients, error, skill);
            if (result != WorldConditionCommit::Applied)
                QueueLogLocked("allied-health", error);
            return result;
        };
    }
    context.persistPreparedAlliedEffects =
        [&](const std::string& deadlines,
            const std::function<bool(const std::vector<unsigned>&, std::string&)>& prepareOwn,
            const std::vector<WorldReceivedEffectTarget>& recipients, std::vector<unsigned>& instances) {
            if (!sequencesReserved || !prepareOwn)
                return WorldConditionCommit::NotApplied;
            std::string error;
            const auto result = SaveAlliedEffectsLocked(state, context, deadlines, std::string(), recipients, instances,
                                                        error, prepareOwn);
            if (result != WorldConditionCommit::Applied)
                QueueLogLocked("allied-effects", error);
            return result;
        };
    context.characterTargetPosition = [&](uint32_t agent, CatalogPosition& position) {
        if (!agent || agent > UINT16_MAX || agent == context.peerAgent || agent == 57344)
            return false;
        for (const auto& pair : sessions_) {
            const auto& peer = pair.second;
            if (&peer != &state && !peer.loggedOut && peer.worldOwner && peer.worldReady && peer.peerAnnounced &&
                peer.peerIdentity && peer.activeWorld &&
                (!peer.outgoingOverflow || targets.Pinned(peer.worldTargets, peer.peerIdentity)) &&
                peer.peerIdentity->character == agent && peer.activeWorld->character.map == character.map)
                if (targets.Pin(peer.worldTargets, peer.peerIdentity)) {
                    const auto& target = peer.activeWorld->character;
                    position = {target.x, target.y, target.z};
                    return true;
                }
        }
        return false;
    };
    context.isVisibleCharacterTarget = [&](uint32_t agent) {
        CatalogPosition position;
        return context.characterTargetPosition(agent, position);
    };
    context.combatTarget = [&](uint32_t agent, WorldCombatTarget& out) {
        if (!state.combatEnabled)
            return false;
        CatalogPosition position;
        if (!context.characterTargetPosition(agent, position))
            return false;
        for (const auto& pair : sessions_) {
            const auto& peer = pair.second;
            if (!peer.combatEnabled || !peer.peerIdentity || peer.peerIdentity->character != agent || !peer.activeWorld)
                continue;
            const auto& target = peer.activeWorld->character;
            WorldCombatTarget value;
            value.actor.agent = peer.peerIdentity->character;
            value.actor.identity = target.guid.Text() + ":" + std::to_string(peer.worldOwner);
            value.actor.position = position;
            value.level = beta::CombatLevel(profile_->Info().build, target);
            const auto health = target.profileFields.find("health_state");
            if (health != target.profileFields.end())
                value.actor.health = health->second;
            const auto conditions = target.profileFields.find("condition_effects");
            if (conditions != target.profileFields.end())
                value.actor.conditions = conditions->second;
            const auto effects = target.profileFields.find("skill_effects");
            if (effects != target.profileFields.end())
                value.positiveEffects = effects->second;
            value.blocking = peer.worldActions && peer.worldActions->BlocksIncoming(context.now);
            value.evading = peer.worldActions && peer.worldActions->EvadesIncoming(context.now);
            out = std::move(value);
            return true;
        }
        return false;
    };
    context.combatEnemies = [&](std::vector<WorldCombatTarget>& out) {
        out.clear();
        for (const auto& pair : sessions_) {
            const auto& peer = pair.second;
            if (&peer == &state || !peer.peerIdentity)
                continue;
            WorldCombatTarget enemy;
            if (context.combatTarget(peer.peerIdentity->character, enemy))
                out.push_back(std::move(enemy));
        }
        return true;
    };
    context.conditionAllies = [&](std::vector<WorldConditionActor>& out) {
        std::vector<WorldConditionActor> result;
        // Announced players are allies unless both opted into combat.
        for (const auto& pair : sessions_) {
            const auto& peer = pair.second;
            if (&peer == &state || !peer.peerIdentity || !peer.activeWorld)
                continue;
            if (state.combatEnabled && peer.combatEnabled)
                continue;
            float targetHealth = 0;
            if (!modern::ReadHealthState(peer.activeWorld->character, targetHealth) || targetHealth <= 0)
                continue;
            CatalogPosition position;
            if (!context.characterTargetPosition(peer.peerIdentity->character, position))
                continue;
            WorldConditionActor actor;
            actor.agent = peer.peerIdentity->character;
            actor.identity = peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner);
            actor.position = position;
            const auto field = peer.activeWorld->character.profileFields.find("condition_effects");
            if (field != peer.activeWorld->character.profileFields.end())
                actor.conditions = field->second;
            const auto health = peer.activeWorld->character.profileFields.find("health_state");
            if (health != peer.activeWorld->character.profileFields.end())
                actor.health = health->second;
            result.push_back(std::move(actor));
        }
        out = std::move(result);
        return true;
    };
    Buffer automaticInput;
    if (forcedAttack) {
        if (!context.taunted || context.stunned || state.forcedAttackIdentity >= 0x7fffffffu)
            return WorldActionResult::Ignored;
        WorldCombatTarget enemy;
        float ownHealth = 0, targetHealth = 0;
        if (!context.combatTarget(forcedSource->character, enemy) ||
            !modern::ReadHealthState(context.healthState, ownHealth) ||
            !modern::ReadHealthState(enemy.actor.health, targetHealth) || !ownHealth || !targetHealth)
            return WorldActionResult::Ignored;
        const auto built = state.worldActions->BasicAttackRequest(
            context, forcedSource->character, 0x80000000u | ++state.forcedAttackIdentity, automaticInput);
        if (built != WorldActionResult::Handled)
            return built;
        // The due-impact gate examines the actual order. An empty view cannot
        // identify a new attack and would let it replace an unapplied volley.
        if (state.worldActions->RequiresRequestTick(automaticInput.View(), context.now))
            return WorldActionResult::Ignored;
        input = automaticInput.View();
    }
    WorldActionOutput output;
    const auto result =
        tick ? state.worldActions->Tick(context, output) : state.worldActions->Request(input, context, output);
    // A tick can expire and persist a condition itself. Observe the resulting
    // durable state, not only the snapshot taken before that tick.
    std::string conditionNow;
    if (Verbose() && tick) {
        const auto storedConditions = state.activeWorld->character.profileFields.find("condition_effects");
        if (storedConditions != state.activeWorld->character.profileFields.end())
            conditionNow = storedConditions->second;
    }
    const bool conditionTransition =
        Verbose() && tick && (state.conditionTraceOwner != state.worldOwner || state.conditionTrace != conditionNow);
    // Bounded native6224 diagnostics: only a durable rock-ledger transition,
    // never account/chat contents or every worker tick. Reuse delivery receipts.
    std::string rockNow;
    const unsigned rockBuild = profile_->Info().build;
    const bool inspectRocks = Verbose() && state.activeWorld &&
                              (rockBuild == 14075 || rockBuild == 14422 || rockBuild == 14505 || rockBuild == 14812);
    if (inspectRocks) {
        const auto rockSaved = state.activeWorld->character.profileFields.find("skill_effects");
        if (rockSaved != state.activeWorld->character.profileFields.end())
            rockNow = rockSaved->second;
    }
    const bool rockTransition =
        inspectRocks && rockNow != context.skillEffects &&
        (rockNow.find(":1384:") != std::string::npos || context.skillEffects.find(":1384:") != std::string::npos);
    if (rockTransition && state.conditionDeliveryCount < 32)
        QueueLogLocked("rock-buffs", "owner=" + std::to_string(state.worldOwner) +
                                         " result=" + std::to_string(static_cast<unsigned>(result)) + " now=" +
                                         std::to_string(context.now) + " utc=" + std::to_string(context.wallNow) +
                                         " ownTail=" + ConditionPacketTail(output.owned));
    if (conditionTransition) {
        // Bounded diagnostics for durable condition transitions. Log the tail
        // containing condition packets, never an entire stream or account.
        QueueLogLocked("condition-sync",
                       "owner=" + std::to_string(state.worldOwner) + " agent=" + std::to_string(context.peerAgent) +
                           " result=" + std::to_string(static_cast<unsigned>(result)) + " state=" + conditionNow +
                           " ownTail=" + ConditionPacketTail(output.owned) +
                           " peerTail=" + ConditionPacketTail(output.peers));
        if (state.conditionTraceOwner != state.worldOwner) {
            state.conditionDeliveries.clear();
            state.conditionDeliveryCount = 0;
        }
        state.conditionTraceOwner = state.worldOwner;
        state.conditionTrace = conditionNow;
    }
    if (result == WorldActionResult::Failed)
        return result;
    const bool emitted = output.owned.Size() || output.peers.Size() || output.targetView.Size();
    if (bool(output.targetPeerAgent) != bool(output.targetView.Size()) ||
        (output.targetView.Size() &&
         (!output.peers.Size() || !context.isVisibleCharacterTarget(output.targetPeerAgent))))
        return WorldActionResult::Failed;
    if (output.sequences > 32 || (sequencesReserved ? (output.sequences != reservedSequences && output.sequences != 0)
                                                    : output.sequences > UINT32_MAX - nextSequence_))
        return WorldActionResult::Failed;
    const bool flightChanged = output.flight != FlightEvent::None;
    if (flightChanged && output.flight != FlightEvent::Deploy && output.flight != FlightEvent::Stop)
        return WorldActionResult::Failed;
    if (result == WorldActionResult::Ignored && (emitted || flightChanged || output.relocate))
        return WorldActionResult::Failed;
    if (output.relocate && (!output.owned.Size() || !output.peers.Size() || !output.sequences ||
                            !std::isfinite(output.destination.x * 32) || !std::isfinite(output.destination.y * 32) ||
                            !std::isfinite(output.destination.z * 32)))
        return WorldActionResult::Failed;
    if (output.peerEffectVersion &&
        (!output.peers.Size() || output.peerEffectVersion != state.worldActions->PeerEffectVersion()))
        return WorldActionResult::Failed;
    if (output.owned.Size()) {
        const std::shared_ptr<const Buffer> packet =
            forcedSource ? PeerMessage(std::move(output.owned), state.peerIdentity, forcedSource)
                         : std::make_shared<Buffer>(std::move(output.owned));
        QueuePeerLocked(state, packet);
        if (state.outgoingOverflow)
            return WorldActionResult::Failed;
        // Retain only bounded condition-transition packets, never arbitrary
        // account/chat traffic. A reference prevents pointer reuse before Send.
        if ((conditionTransition || rockTransition) && packet->Size() <= 4096 && state.conditionDeliveryCount < 32) {
            SessionState::ConditionDelivery delivery;
            delivery.message = packet;
            delivery.owner = state.worldOwner;
            delivery.trace = ++state.conditionDeliveryCount;
            state.conditionDeliveries.push_back(std::move(delivery));
        }
    }
    if (output.peers.Size())
        BroadcastPeerLocked(state, std::move(output.peers), output.targetPeerAgent, std::move(output.targetView),
                            output.peerEffectVersion, forcedSource);
    if (output.relocate) {
        auto& live = state.activeWorld->character;
        live.x = output.destination.x;
        live.y = output.destination.y;
        live.z = output.destination.z;
        state.flightMotion.Clear();
    }
    if (flightChanged) {
        state.flying = output.flight == FlightEvent::Deploy;
        state.flightMotion.Clear();
        state.flightRelayAt = context.now;
        if (Verbose())
            QueueLogLocked("flight", "agent=" + std::to_string(context.peerAgent) +
                                         " active=" + std::to_string(state.flying ? 1 : 0));
    }
    if (emitted && !sequencesReserved)
        nextSequence_ += output.sequences;
    return result;
}
void Server::TraceWorldDelivery(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                const std::shared_ptr<const Buffer>& message, bool transportAccepted) {
    if (!Verbose() || !session || !message)
        return;
    StateGuard guard(*this);
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session)
        return;
    auto& pending = at->second.conditionDeliveries;
    for (auto i = pending.begin(); i != pending.end(); ++i) {
        if (i->owner != owner || i->message != message)
            continue;
        QueueLogLocked("condition-send", "owner=" + std::to_string(owner) + " trace=" + std::to_string(i->trace) +
                                             " transport=" + (transportAccepted ? "accepted" : "failed") +
                                             " bytes=" + std::to_string(message->Size()) +
                                             " tail=" + ConditionPacketTail(*message));
        pending.erase(i);
        return;
    }
}
bool Server::RelayWorldMotion(const std::shared_ptr<const LoginSession>& session, uint64_t owner, ByteView motion) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !owner || !profile_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld ||
        !at->second.peerIdentity || at->second.outgoingOverflow || nextSequence_ > UINT32_MAX - 2)
        return false;
    auto& state = at->second;
    const auto& movementGame = *state.gameMessages;
    Facing movementFacing;
    CatalogPosition reported;
    const auto facingStatus = movementGame.DecodeFacing(motion, movementFacing);
    const auto positionStatus = movementGame.DecodePosition(motion, reported);
    if (facingStatus == FacingResult::Invalid || positionStatus == PositionResult::Invalid)
        return false;
    // A cached template must be usable by the already verified relocation path.
    // It belongs only to this world session and is never taken from a peer.
    if (facingStatus == FacingResult::Present && motion.size <= 4096) {
        Buffer probe;
        const auto& c = state.activeWorld->character;
        if (movementGame.TeleportNeedsMotion() &&
            movementGame.LocalTeleport(motion, {c.x, c.y, c.z}, nextSequence_, probe)) {
            Buffer copy;
            if (!copy.Append(motion))
                return false;
            state.controlMotion = std::move(copy);
        }
    }
    combat::StunState control;
    if (!control.Load(profile_->Info().build, combat::StunState::Field(state.activeWorld->character)))
        return false;
    if (control.Active(wallClock_()) &&
        !combat::StunBreakRequest(profile_->Info().build, state.peerIdentity->character, motion)) {
        const auto now = clock_();
        if (positionStatus == PositionResult::Present || facingStatus == FacingResult::Present) {
            const bool due = !state.controlCorrectionSent || now < state.controlCorrectionAt ||
                             now - state.controlCorrectionAt >= 50;
            if (due && (!movementGame.TeleportNeedsMotion() || state.controlMotion.Size())) {
                const uint32_t span = profile_->Info().build == 49284 ? 1u : 2u;
                if (nextSequence_ > UINT32_MAX - 2 * span)
                    return false;
                const auto& c = state.activeWorld->character;
                const CatalogPosition held{c.x, c.y, c.z};
                Buffer owned, peers;
                if (!movementGame.LocalTeleport(state.controlMotion.View(), held, nextSequence_, owned) ||
                    !movementGame.PeerTeleport(state.controlMotion.View(), held, state.peerIdentity->character,
                                               nextSequence_ + span, peers))
                    return false;
                nextSequence_ += 2 * span;
                QueuePeerLocked(state, std::make_shared<Buffer>(std::move(owned)));
                BroadcastPeerLocked(state, std::move(peers));
                state.controlCorrectionAt = now;
                state.controlCorrectionSent = true;
            }
        }
        return !state.outgoingOverflow;
    }
    state.controlCorrectionSent = false;
    // Settle due gains/cleanses before a debit; the subsequent request gets a
    // fresh context after any completed durable transaction.
    if (state.gameMessages->NeedsActionTickBeforeMotion(motion) &&
        RunWorldActionsLocked(state, ByteView(), true) == WorldActionResult::Failed)
        return false;
    const auto actionResult = RunWorldActionsLocked(state, motion, false);
    if (actionResult != WorldActionResult::Ignored) {
        if (actionResult != WorldActionResult::Handled)
            return false;
        Facing direction;
        const auto facing = state.gameMessages->DecodeFacing(motion, direction);
        if (facing == FacingResult::Invalid)
            return false;
        if (facing == FacingResult::Present) {
            state.facing = direction;
            state.hasFacing = true;
        }
        return true;
    }
    const auto now = clock_();
    const auto& game = *state.gameMessages;
    Buffer message;
    auto result = game.PeerMotion(motion, state.peerIdentity->character, nextSequence_,
                                  static_cast<uint32_t>(now & 0x7fffffff), message);
    if (result == RelayResult::Invalid)
        return false;
    Facing direction;
    const auto facingResult = game.DecodeFacing(motion, direction);
    if (facingResult == FacingResult::Invalid)
        return false;
    const auto flight = game.DecodeFlight(motion);
    if (flight.event == FlightEvent::Invalid)
        return false;
    Buffer ownMessage;
    const auto ownResult = game.OwnMotion(motion, nextSequence_, static_cast<uint32_t>(now & 0x7fffffff), ownMessage);
    if (ownResult == RelayResult::Invalid)
        return false;
    if (facingResult == FacingResult::Present) {
        state.facing = direction;
        state.hasFacing = true;
    }
    switch (flight.event) {
    case FlightEvent::Invalid:
        return false;
    case FlightEvent::Deploy:
        if (state.mountSkin) {
            Buffer notice;
            if (!game.ServiceChat("Dismount before deploying the glider.", state.activeWorld->character.guid, notice))
                return false;
            QueuePeerLocked(state, std::make_shared<Buffer>(std::move(notice)));
            return true;
        }
        state.flying = true;
        state.flightMotion.Clear();
        state.flightRelayAt = now;
        break;
    case FlightEvent::Stop:
        state.flying = false;
        state.flightMotion.Clear();
        break;
    case FlightEvent::DetailedMotion:
        if (state.flying) {
            state.flightMotion.Clear();
            if (!state.flightMotion.Append(motion))
                return false;
            state.flightCounter = flight.counter;
            state.flightRelayAt = now;
        }
        break;
    case FlightEvent::Position:
        if (state.flying && state.flightMotion.Size() && now - state.flightRelayAt >= 30) {
            const uint16_t distance = static_cast<uint16_t>(flight.counter - state.flightCounter);
            if (distance && distance < 0x8000) {
                result = game.PeerGlide(state.flightMotion.View(), motion, state.peerIdentity->character, nextSequence_,
                                        message);
                if (result == RelayResult::Invalid)
                    return false;
                if (result == RelayResult::Encoded) {
                    state.flightCounter = flight.counter;
                    state.flightRelayAt = now;
                }
            }
        }
        break;
    case FlightEvent::None:
        break;
    }
    if (ownResult == RelayResult::Encoded) {
        QueuePeerLocked(state, std::make_shared<Buffer>(std::move(ownMessage)));
        if (state.outgoingOverflow)
            return false;
    }
    if (result == RelayResult::Encoded || ownResult == RelayResult::Encoded)
        nextSequence_ += 2;
    if (result == RelayResult::Encoded) {
        BroadcastPeerLocked(at->second, std::move(message));
    }
    return true;
}
bool Server::ApplyWorldAction(const std::shared_ptr<const LoginSession>& session, uint64_t owner, ByteView input) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !owner || !profile_ || !input.Valid() || input.size < 2)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld ||
        !at->second.peerIdentity || at->second.outgoingOverflow)
        return false;
    return RunWorldActionsLocked(at->second, input, false) == WorldActionResult::Handled;
}
} // namespace glide
