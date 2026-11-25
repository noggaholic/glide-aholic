#include "glide/crypto.h"
#include "glide/profile/combat_traits.h"
#include "glide/profile/assassin_siphon.h"
#include "glide/profile/boon_removal.h"
#include "glide/profile/jade_winds.h"
#include "glide/profile/reveal_state.h"
#include "glide/profile/condition_damage.h"
#include "glide/profile/forced_engagement.h"
#include "server.h"
#include <cmath>
#include <cstring>

namespace glide {
bool Server::IssueWorld(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, WorldEntry& out,
                        std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    return IssueWorldLocked(session, issuer, nullptr, out, nullptr, error);
}
bool Server::TransferWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                           const CatalogPosition& current, unsigned map, const CatalogPosition& destination,
                           WorldEntry& out, Buffer& redirect, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    return TransferWorldLocked(session, owner, current, map, destination, out, redirect, error);
}
bool Server::TransferWorldLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                 const CatalogPosition& current, unsigned map, const CatalogPosition& destination,
                                 WorldEntry& out, Buffer& redirect, std::string& error) {
    error = "Cannot transfer this world connection";
    if (!session || !owner || !profile_ || !accounts_ || !catalog_.Available(map) ||
        !std::isfinite(destination.x * 32) || !std::isfinite(destination.y * 32) || !std::isfinite(destination.z * 32))
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld)
        return false;
    // Finish actions against this world before preparing a redirect. Failed
    // preparation makes the existing world available as a target again.
    WorldTargetExit targetExit(*this, at->second.worldTargets);
    WorldEntry entry = *at->second.activeWorld;
    uint32_t number;
    if (!RandomBytes(reinterpret_cast<uint8_t*>(&number), sizeof(number)))
        return false;
    number = 0xa0000000u | (number & 0x0fffffffu);
    for (const auto& pair : sessions_)
        if (pair.second.createAssociation == number || pair.second.worldEntry.association == number ||
            (pair.second.activeWorld && pair.second.activeWorld->association == number))
            return false;
    Buffer message;
    if (!at->second.gameMessages || !at->second.gameMessages->MapRedirect(number, map, message) ||
        !SaveWorldPositionLocked(session, owner, current, error))
        return false;
    // Other accounts may issue tickets while the position is being saved.
    // Confirm uniqueness again before publishing the prepared destination.
    for (const auto& pair : sessions_)
        if (pair.second.createAssociation == number || pair.second.worldEntry.association == number ||
            (pair.second.activeWorld && pair.second.activeWorld->association == number)) {
            error = "Cannot reserve transfer identifier";
            return false;
        }
    // The last completed world is durable before authorizing a provisional destination.
    entry.character = at->second.activeWorld->character;
    for (auto& character : entry.account.characters)
        if (character.slot == entry.character.slot) {
            character = entry.character;
            character.x = current.x;
            character.y = current.y;
            character.z = current.z;
        }
    entry.association = number;
    entry.transfer = true;
    if (at->second.worldBound)
        entry.joinContext = 0; // Native redirect carries a zero context.
    entry.previousMap = entry.character.map;
    entry.character.map = map;
    entry.character.x = destination.x;
    entry.character.y = destination.y;
    entry.character.z = destination.z;
    auto& state = at->second;
    LeavePeersLocked(state);
    state.outgoing.clear();
    state.outgoingBytes = 0;
    state.outgoingOverflow = false;
    state.worldEntry = entry;
    state.worldExpires = clock_() + 60000;
    state.worldIssuer = owner; // Preserve the active session identity checks.
    state.worldOwner = 0;
    state.worldReady = false;
    state.worldPrepared = false;
    state.activeWorld.reset();
    state.gameMessages.reset();
    state.worldActions.reset();
    // Keep the old owner registered until its worker exits, but deny it any further world writes.
    out = std::move(entry);
    redirect = std::move(message);
    error.clear();
    return true;
}
bool Server::CompleteCreate(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const Guid& character,
                            Buffer& redirect, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    WorldEntry entry;
    const auto* initializer = profile_ ? profile_->GameInitializers() : nullptr;
    return IssueWorldLocked(session, owner, &character, entry, &redirect, error, initializer != nullptr, 0, 0,
                            initializer, Port());
}
bool Server::CompleteBoundCreate(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                 const Guid& character, const GameInitializerCodec& codec, uint16_t port,
                                 Buffer& redirect, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    WorldEntry entry;
    return IssueWorldLocked(session, owner, &character, entry, &redirect, error, true, 0, 0, &codec, port);
}
bool Server::IssueWorldLocked(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, const Guid* created,
                              WorldEntry& out, Buffer* redirect, std::string& error, bool bound, unsigned requestedMap,
                              uint32_t context, const GameInitializerCodec* initializer, uint16_t port) {
    error = "Cannot authorize world entry for this session";
    if (!session || !profile_ || !accounts_ || !issuer)
        return false;
    auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        !at->second.owners.count(issuer) || at->second.worldOwner)
        return false;
    const bool authAfterCreate = !created && at->second.createOwner != 0;
    if (created && at->second.createOwner != issuer)
        return false;
    if (created && bound &&
        (!initializer || !port || !at->second.createBound || !at->second.createSaved ||
         std::memcmp(at->second.createdCharacter.bytes, created->bytes, 16) ||
         std::memcmp(at->second.createConfirmed.bytes, created->bytes, 16) ||
         !at->second.owners.count(at->second.createIssuer)))
        return false;
    if (authAfterCreate && (!Creation().WorldRequestThroughAuth() || !at->second.createSaved ||
                            at->second.createIssuer != issuer || at->second.worldEntry.association))
        return false;
    WorldEntry entry;
    KvDocument document;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, entry.account, error))
        return false;
    const Character* selected = nullptr;
    for (const auto& character : entry.account.characters)
        if (character.slot == entry.account.selected)
            selected = &character;
    if (!selected) {
        error = "No selected character";
        return false;
    }
    if (authAfterCreate && !at->second.createdCharacter.Matches(ByteView(selected->guid.bytes, 16))) {
        error = "Only the newly saved character can enter during creation";
        return false;
    }
    if (created && !created->Matches(ByteView(selected->guid.bytes, 16))) {
        error = "Created character is no longer selected";
        return false;
    }
    if (!catalog_.Available(selected->map)) {
        error = "Saved character map is unavailable";
        return false;
    }
    Buffer summary;
    const CharacterCodec& codec =
        characterCodec_ ? *characterCodec_ : static_cast<const CharacterCodec&>(profile_->Auth());
    if (!codec.Summary(*selected, summary)) {
        error = "Invalid saved character appearance";
        return false;
    }
    unsigned combatTrait = 0;
    if (!combat::ReadCombatTrait(profile_->Info().build, *selected, combatTrait)) {
        error = "Invalid saved combat trait selection";
        return false;
    }
    combat::AssassinSiphon siphon;
    if (!siphon.Load(profile_->Info().build, *selected)) {
        error = "Invalid saved assassin siphon";
        return false;
    }
    combat::BoonRemovals removedBoons;
    if (!removedBoons.Load(combat::BoonRemovals::Field(*selected))) {
        error = "Invalid saved boon removals";
        return false;
    }
    combat::StunState control;
    if (!control.Load(profile_->Info().build, combat::StunState::Field(*selected))) {
        error = "Invalid saved control state";
        return false;
    }
    combat::RevealState revealed;
    if (!revealed.Load(profile_->Info().build, combat::RevealState::Field(*selected))) {
        error = "Invalid saved reveal";
        return false;
    }
    {
        const auto damageClockField = selected->profileFields.find("condition_damage_clock");
        if (damageClockField != selected->profileFields.end() &&
            !combat::ConditionDamage::ValidClock(profile_->Info().build, damageClockField->second)) {
            error = "Invalid saved condition damage clock";
            return false;
        }
    }
    combat::TauntState taunt;
    if (!taunt.Load(profile_->Info().build, combat::TauntState::Field(*selected))) {
        error = "Invalid saved taunt";
        return false;
    }
    entry.character = *selected;
    if (bound) {
        Guid user;
        if (!Guid::Parse(session->userId, user) || std::memcmp(user.bytes, entry.account.userGuid.bytes, 16))
            return false;
        if (requestedMap && requestedMap != selected->map) {
            SpawnPoint destination{};
            if (created || authAfterCreate ||
                !profile_->SavedMapReturn(selected->map, requestedMap, context, destination) ||
                destination.map != requestedMap || !catalog_.Available(destination.map) ||
                !std::isfinite(destination.x * 32) || !std::isfinite(destination.y * 32) ||
                !std::isfinite(destination.z * 32))
                return false;
            // Authorize a provisional native return. Disk retains the saved map
            // until WorldReady; ClaimBoundWorld validates that original snapshot.
            entry.transfer = true;
            entry.previousMap = selected->map;
            entry.character.map = destination.map;
            entry.character.x = destination.x;
            entry.character.y = destination.y;
            entry.character.z = destination.z;
        }
        entry.joinContext = context;
    }
    uint32_t number;
    if (!RandomBytes(reinterpret_cast<uint8_t*>(&number), sizeof(number)))
        return false;
    number = 0xa0000000u | (number & 0x0fffffffu);
    for (const auto& pair : sessions_)
        if (pair.second.createAssociation == number || pair.second.worldEntry.association == number)
            return false;
    entry.association = number;
    if (created) {
        entry.transfer = true;
        if (!redirect ||
            !(bound ? initializer->Redirect(number, static_cast<uint16_t>(entry.character.map), port, *redirect)
                    : profile_->Game().MapRedirect(number, entry.character.map, *redirect)))
            return false;
        at->second.createOwner = 0;
    }
    at->second.worldEntry = entry;
    at->second.worldExpires = clock_() + 60000;
    at->second.worldIssuer = issuer;
    at->second.worldBound = bound;
    if (created && bound)
        at->second.worldIssuer = at->second.createIssuer;
    at->second.createAssociation = 0;
    out = std::move(entry);
    error.clear();
    return true;
}
std::shared_ptr<const LoginSession> Server::ClaimWorld(uint64_t owner, uint32_t association, unsigned map,
                                                       uint32_t context, ByteView prefix, WorldEntry& out) {
    if (!owner || !association || !prefix.Valid() || prefix.size != 4)
        return nullptr;
    std::shared_ptr<const LoginSession> candidate;
    {
        StateGuard guard(*this);
        Prune();
        candidate = FindClaimSessionLocked(association, false, false);
    }
    if (!candidate)
        return nullptr;
    AccountOperation accountOperation(*this, candidate->login);
    StateGuard guard(*this);
    Prune();
    return ClaimWorldLocked(owner, association, map, context, prefix, out, false, {}, {}, candidate);
}
bool Server::IssueBoundWorld(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, unsigned requestedMap,
                             uint32_t context, WorldEntry& out, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    return IssueWorldLocked(session, issuer, nullptr, out, nullptr, error, true, requestedMap, context);
}
std::shared_ptr<const LoginSession> Server::ClaimBoundWorld(uint64_t owner, uint32_t association, unsigned map,
                                                            uint32_t context, ByteView user, ByteView character,
                                                            WorldEntry& out) {
    if (!owner || !association || !user.Valid() || user.size != 16 || !character.Valid() || character.size != 16)
        return nullptr;
    const uint8_t prefix[4] = {};
    std::shared_ptr<const LoginSession> candidate;
    {
        StateGuard guard(*this);
        Prune();
        candidate = FindClaimSessionLocked(association, false, true);
    }
    if (!candidate)
        return nullptr;
    AccountOperation accountOperation(*this, candidate->login);
    StateGuard guard(*this);
    Prune();
    return ClaimWorldLocked(owner, association, map, context, ByteView(prefix, 4), out, true, user, character,
                            candidate);
}
std::shared_ptr<const LoginSession>
Server::ClaimWorldLocked(uint64_t owner, uint32_t association, unsigned map, uint32_t context, ByteView prefix,
                         WorldEntry& out, bool bound, ByteView user, ByteView character,
                         const std::shared_ptr<const LoginSession>& expectedSession) {
    // A new GAME connection owns no other session or protocol connection.
    for (const auto& pair : sessions_)
        if (pair.second.owners.count(owner))
            return nullptr;
    AdmissionReservation reservation(*this, owner, expectedSession);
    if (!reservation.Reserved())
        return nullptr;
    for (auto& pair : sessions_) {
        auto& state = pair.second;
        if (state.session != expectedSession || state.loggedOut || state.worldBound != bound ||
            state.worldEntry.association != association || state.worldExpires <= clock_() || state.worldOwner ||
            state.createOwner || state.worldEntry.character.map != map)
            continue;
        if (bound) {
            // During transfer the destination is provisional; disk still holds
            // the last completed map. Validate against that authorized snapshot.
            const Character* expected = &state.worldEntry.character;
            if (state.worldEntry.transfer && state.worldEntry.previousMap) {
                expected = nullptr;
                for (const auto& candidate : state.worldEntry.account.characters)
                    if (candidate.slot == state.worldEntry.character.slot)
                        expected = &candidate;
                if (!expected || expected->map != state.worldEntry.previousMap)
                    return nullptr;
            }
            const auto& saved = *expected;
            if (state.worldEntry.joinContext != context) {
                QueueLogLocked("world", "Bound initializer context mismatch");
                return nullptr;
            }
            if (std::memcmp(state.worldEntry.account.userGuid.bytes, user.data, 16) ||
                std::memcmp(saved.guid.bytes, character.data, 16)) {
                QueueLogLocked("world", "Bound initializer identity mismatch");
                return nullptr;
            }
            KvDocument document;
            AccountState current;
            std::string error;
            if (!accounts_ || !LoadAccountLocked(state.session->login, document, error) ||
                !DecodeAccountState(document, current, error) || std::memcmp(current.userGuid.bytes, user.data, 16))
                return nullptr;
            const Character* selected = nullptr;
            for (const auto& candidate : current.characters)
                if (candidate.slot == current.selected)
                    selected = &candidate;
            if (!selected || std::memcmp(selected->guid.bytes, saved.guid.bytes, 16) || selected->slot != saved.slot ||
                selected->name != saved.name || selected->map != saved.map || selected->level != saved.level ||
                selected->x != saved.x || selected->y != saved.y || selected->z != saved.z ||
                selected->glider != saved.glider || selected->mount != saved.mount ||
                selected->profileFields != saved.profileFields || selected->native != saved.native) {
                QueueLogLocked("world", "Bound initializer saved character mismatch");
                return nullptr;
            }
        }
        if (state.worldExpires <= clock_())
            return nullptr;
        for (const auto& existing : sessions_)
            if (existing.second.owners.count(owner))
                return nullptr;
        WorldEntry entry = state.worldEntry;
        // These are opaque GAME initializer fields, not an authenticated account GUID.
        entry.joinContext = context;
        std::memcpy(entry.joinPrefix, prefix.data, 4);
        state.outgoing.clear();
        state.outgoingBytes = 0;
        state.outgoingOverflow = false;
        state.savedMap = 0;
        for (const auto& stored : entry.account.characters)
            if (stored.slot == entry.character.slot)
                state.savedMap = stored.map;
        state.gameMessages.reset();
        state.worldActions.reset();
        state.activeWorld.reset(new WorldEntry(entry));
        state.conditionDamageReady = false;
        state.conditionDamagePending = false;
        state.controlMotion.Clear();
        state.controlCorrectionAt = 0;
        state.controlCorrectionSent = false;
        state.worldEntry = WorldEntry();
        state.worldPrepared = false;
        state.worldReady = false;
        state.worldOwner = owner;
        state.owners.insert(owner);
        out = std::move(entry);
        return state.session;
    }
    return nullptr;
}
bool Server::PrepareWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                          std::vector<BootstrapStep>& out, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    error = "Cannot prepare world for this connection";
    if (!session || !owner || !profile_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.activeWorld || at->second.worldPrepared ||
        nextSequence_ > UINT32_MAX - 10)
        return false;
    auto& slots = peerSlots_[profile_->GlobalPeerIds() ? 0 : at->second.activeWorld->character.map];
    std::shared_ptr<const PeerIdentity> identity;
    size_t freeSlot = 0;
    for (unsigned i = 0;; ++i) {
        PeerIdentity candidate;
        if (!Bootstrap().PeerSlot(i, candidate)) {
            error = "No character identifiers available";
            return false;
        }
        if (i >= slots.size() || slots[i].expired()) {
            if (!Bootstrap().BindSession(owner, candidate)) {
                error = "Cannot reserve session object identifiers";
                return false;
            }
            identity = std::make_shared<PeerIdentity>(candidate);
            freeSlot = i;
            break;
        }
    }
    BootstrapContext context{*at->second.activeWorld, catalog_};
    context.identity = identity.get();
    context.serverPort = Port();
    context.sequence = nextSequence_;
    context.timeTicks = static_cast<uint32_t>((timeTicks_ + (clock_() - timeAnchor_) % 7200000) % 7200000);
    std::shared_ptr<const GameMessages> messages;
    if (!profile_->CreateGameMessages(context, messages, error))
        return false;
    if (!messages) {
        error = "Missing game messages for world connection";
        return false;
    }
    std::unique_ptr<WorldActions> actions;
    if (!profile_->CreateWorldActions(context, config_.dataDir, actions, error))
        return false;
    if (!Bootstrap().Steps(context, out)) {
        error = "Cannot encode saved character world entry";
        return false;
    }
    if (slots.size() <= freeSlot)
        slots.resize(freeSlot + 1);
    slots[freeSlot] = identity;
    at->second.peerIdentity = std::move(identity);
    at->second.peerAnnounced = false;
    at->second.worldTargets = std::make_shared<WorldTargetState>();
    at->second.gameMessages = std::move(messages);
    at->second.worldActions = std::move(actions);
    at->second.worldPrepared = true;
    nextSequence_ += 10;
    error.clear();
    return true;
}
std::shared_ptr<const GameMessages> Server::WorldMessages(const std::shared_ptr<const LoginSession>& session,
                                                          uint64_t owner) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !owner)
        return {};
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.activeWorld || !at->second.worldPrepared)
        return {};
    return at->second.gameMessages;
}
bool Server::WorldReady(const std::shared_ptr<const LoginSession>& session, uint64_t owner, Buffer* warning) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !owner)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.activeWorld || !at->second.worldPrepared || at->second.worldReady)
        return false;
    if (warning)
        warning->Clear();
    if (at->second.activeWorld->transfer) {
        const auto& character = at->second.activeWorld->character;
        std::string error;
        if (!SaveWorldPositionLocked(session, owner, {character.x, character.y, character.z}, error, true)) {
            QueueLogLocked("game", error);
            const std::string notice = "Your new map position could not be saved. You may return to the previous "
                                       "location next time you sign in.";
            if (!warning || !at->second.gameMessages->ServiceChat(notice, character.guid, *warning)) {
                at->second.worldReady = false;
                return false;
            }
        }
    }
    // A transferring player is visible only after its position save and any
    // failure notice have completed. Other accounts can run during that I/O.
    at->second.worldReady = true;
    if (!JoinPeersLocked(at->second)) {
        at->second.worldReady = false;
        return false;
    }
    return true;
}
} // namespace glide
