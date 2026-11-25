#include "commands.h"
#include "server.h"
#include "glide/profile/combat_traits.h"
#include "glide/profile/dragon_facet_data.h"
#include <algorithm>
#include <cmath>

namespace glide {
bool Server::CommandReplies(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const ChatMessage& chat,
                            const CatalogPosition* position, const Facing* facing, ByteView motion,
                            std::vector<Buffer>& replies, CommandResult& command) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !profile_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld)
        return false;
    const auto game = at->second.gameMessages;
    if (!game)
        return false;
    const auto character = at->second.activeWorld->character;
    CommandContext context{catalog_, character,
                           position ? *position : CatalogPosition{character.x, character.y, character.z}};
    context.facing = facing;
    context.capabilities = profile_->Capabilities();
    context.creationSpawn = Creation().InitialLocation();
    context.defaultMapArrival = [this](unsigned map, SpawnPoint& out) { return profile_->DefaultMapArrival(map, out); };
    context.mountSkin = at->second.mountSkin;
    context.flying = at->second.flying;
    context.combatEnabled = at->second.combatEnabled;
    context.timeTicks = static_cast<uint32_t>((timeTicks_ + (clock_() - timeAnchor_) % 7200000) % 7200000);
    for (const auto& pair : sessions_) {
        const auto& peer = pair.second;
        if (peer.loggedOut || !peer.worldOwner || !peer.worldReady || !peer.activeWorld)
            continue;
        const auto& live = peer.activeWorld->character;
        OnlinePlayer player;
        player.combatEnabled = peer.combatEnabled && peer.peerAnnounced && !peer.outgoingOverflow;
        player.name = live.name;
        player.map = live.map;
        player.position = &peer == &at->second ? context.position : CatalogPosition{live.x, live.y, live.z};
        player.hasFacing = peer.hasFacing;
        player.facing = peer.facing;
        if (&peer == &at->second && facing) {
            player.hasFacing = true;
            player.facing = *facing;
        }
        player.glider = live.glider;
        player.mountSkin = peer.mountSkin;
        player.flying = peer.flying;
        context.players.push_back(std::move(player));
    }
    context.online = context.players.size();
    CommandResult output;
    if (!ExecuteCommand(context, chat.text, output)) {
        replies.clear();
        command = {};
        return RelayChatLocked(session, owner, chat);
    }
    return ApplyCommandLocked(session, owner, context.position, motion, std::move(output), replies, command);
}
bool Server::WaypointReplies(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint32_t waypoint,
                             ByteView motion, std::vector<Buffer>& replies, CommandResult& command) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !profile_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld)
        return false;
    const auto& character = at->second.activeWorld->character;
    CommandResult output;
    const auto found = catalog_.waypoints.find(waypoint);
    if (found == catalog_.waypoints.end() || !catalog_.Available(found->second.map)) {
        output.lines.emplace_back("Waypoint destination is not available in the local catalog.");
    } else if (found->second.noCrossMap && found->second.map != character.map) {
        output.lines.emplace_back("That waypoint does not allow travel from another map.");
    } else {
        if (!catalog_.WorldPosition(found->second.map, waypoint, output.destination))
            return false;
        const auto& game = at->second.gameMessages;
        if (!game)
            return false;
        // Fresh modern sessions have no movement template. Reuse the existing
        // map-transfer entry instead of requiring a preliminary key press.
        output.teleport = found->second.map == character.map && (motion.size || !game->TeleportNeedsMotion());
        output.transferMap = output.teleport ? 0 : found->second.map;
    }
    return ApplyCommandLocked(session, owner, CatalogPosition{character.x, character.y, character.z}, motion,
                              std::move(output), replies, command);
}
bool Server::ApplyCommandLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                const CatalogPosition& position, ByteView motion, CommandResult output,
                                std::vector<Buffer>& replies, CommandResult& command) {
    const auto at = sessions_.find(session->login);
    const auto game = at->second.gameMessages;
    if (!game)
        return false;
    const auto character = at->second.activeWorld->character;
    std::vector<Buffer> result;
    if (output.facetSkill || output.selectFacetLegend) {
        const unsigned build = profile_->Info().build;
        const auto profession = character.profileFields.find("profession");
        if ((build != 167409 && build != 204489) || profession == character.profileFields.end() ||
            profession->second != "9")
            return false;
        Buffer input;
        if (output.selectFacetLegend) {
            if (!input.U16(build == 167409 ? 180 : 192) || !input.U8(0) || !input.U8(build == 167409 ? 14 : 15) ||
                !input.U8(output.facetWater ? 1 : 0))
                return false;
        } else {
            const auto rule = build == 167409 ? modern::FacetData<167409>::RuleFor(output.facetSkill)
                                              : modern::FacetData<204489>::RuleFor(output.facetSkill);
            if (!rule.base || !nextSequence_ || nextSequence_ == UINT32_MAX)
                return false;
            // Existing C23 explicit-skill request, with the build's own slot mapping.
            if (!input.U16(23) || !input.U16(57344) || !input.VarU32(1) || !input.U8(3) ||
                !input.VarU32(0x80000000u | nextSequence_) || !input.U8(10) || !input.U8(35) || !input.U32(0) ||
                !input.U32(output.facetSkill) || !input.U8(static_cast<uint8_t>(rule.slot)))
                return false;
        }
        if (RunWorldActionsLocked(at->second, input.View(), false) == WorldActionResult::Failed)
            return false;
    }
    if (output.changeTrait) {
        std::string error;
        if (!SaveWorldCombatTraitLocked(at->second, output.combatTrait, error)) {
            output.changeTrait = false;
            output.lines.assign(1, "Trait selection could not be saved. Try again.");
            unsigned current = 0;
            combat::ReadCombatTrait(profile_->Info().build, at->second.activeWorld->character, current);
            output.combatTrait = current;
            QueueLogLocked("combat-trait", error);
        }
    }
    if (output.markerAction != MarkerAction::None)
        MarkerRepliesLocked(session, owner, position, output);
    if (!output.announcement.empty()) {
        ChatMessage announcement;
        announcement.text = output.announcement;
        if (!RelayChatLocked(session, owner, announcement, true))
            return false;
    }
    if (output.travelTargets != TravelTargets::None) {
        if (!catalog_.Available(output.requestedMap) || !std::isfinite(output.destination.x * 32) ||
            !std::isfinite(output.destination.y * 32) || !std::isfinite(output.destination.z * 32))
            return false;
        size_t requested = 0, busy = 0;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (peer.loggedOut || !peer.worldOwner || !peer.worldReady || !peer.activeWorld || peer.outgoingOverflow)
                continue;
            if (output.travelTargets == TravelTargets::Others && &peer == &at->second)
                continue;
            if (output.travelTargets == TravelTargets::Player &&
                peer.activeWorld->character.name != output.travelPlayer)
                continue;
            if (peer.requestedMap) {
                ++busy;
                continue;
            }
            peer.requestedMap = output.requestedMap;
            peer.requestedPosition = output.destination;
            ++requested;
        }
        output.lines.emplace_back("Transfer requested for " + std::to_string(requested) + " player(s) to map " +
                                  std::to_string(output.requestedMap) + ".");
        if (busy)
            output.lines.emplace_back(std::to_string(busy) + " player(s) already have a pending transfer.");
    }
    if (output.changeTime) {
        Buffer update;
        if (!game->TimeOfDay(output.timeTicks, 1, false, update))
            return false;
        const auto shared = std::make_shared<Buffer>();
        if (!shared->Append(update.View()))
            return false;
        timeTicks_ = output.timeTicks;
        timeAnchor_ = clock_();
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            // Prepared clients drain this after their initial, possibly older, clock packet.
            if (&peer != &at->second && !peer.loggedOut && peer.worldOwner && peer.worldPrepared && peer.activeWorld)
                QueuePeerLocked(peer, shared);
        }
        result.push_back(std::move(update));
    }
    if (output.transferMap) {
        WorldEntry entry;
        Buffer redirect;
        std::string error;
        if (!TransferWorldLocked(session, owner, position, output.transferMap, output.destination, entry, redirect,
                                 error)) {
            QueueLogLocked("travel", error);
            output.transferMap = 0;
            output.lines.emplace_back("The map change could not be started. Your saved destination has not changed.");
        } else
            result.push_back(std::move(redirect));
    }
    if (output.glider) {
        auto& state = at->second;
        Buffer local, remote;
        std::string error;
        if (!state.peerIdentity || !game->Glider(output.glider, 0, local) ||
            !game->Glider(output.glider, state.peerIdentity->character, remote))
            return false;
        if (!SetGliderLocked(session, owner, output.glider, error)) {
            QueueLogLocked("glider", error);
            output.glider = 0;
            output.lines.emplace_back("The glider could not be saved. Your previous choice is unchanged.");
        } else {
            BroadcastPeerLocked(state, std::move(remote));
            result.push_back(std::move(local));
            output.lines.emplace_back("Glider design set to " + std::to_string(output.glider) +
                                      ". Redeploy the glider to see it.");
        }
    }
    if (output.changeMount) {
        auto& state = at->second;
        if (!state.peerIdentity)
            return false;
        Buffer local, remote, removeLocal, removeRemote;
        uint32_t definition = 0, skinContent = 0;
        if (state.mountSkin) {
            if (!game->Unmount(0, removeLocal) || !game->Unmount(state.peerIdentity->character, removeRemote))
                return false;
        }
        if (output.mountSkin) {
            const auto found = catalog_.mounts.find(output.mountSkin);
            if (found == catalog_.mounts.end() || !found->second.mount || state.flying)
                return false;
            definition = found->second.mount;
            skinContent = found->second.content;
            if (!game->MountStart(skinContent, definition, 0, local) ||
                !game->MountStart(skinContent, definition, state.peerIdentity->character, remote))
                return false;
        }
        state.mountSkin = output.mountSkin;
        state.mountDefinition = definition;
        state.mountContent = skinContent;
        // Preserve the legacy removal/association frame boundary and order.
        if (removeLocal.Size()) {
            BroadcastPeerLocked(state, std::move(removeRemote));
            result.push_back(std::move(removeLocal));
        }
        if (local.Size()) {
            BroadcastPeerLocked(state, std::move(remote));
            result.push_back(std::move(local));
        }
    }
    if (output.teleport) {
        Buffer teleport;
        if (!motion.size && game->TeleportNeedsMotion()) {
            output.teleport = false;
            output.lines.emplace_back("Move the character once, then retry the teleport.");
        } else {
            Buffer remote;
            auto& state = at->second;
            if (nextSequence_ > UINT32_MAX - 2 || !state.peerIdentity ||
                !game->LocalTeleport(motion, output.destination, nextSequence_, teleport) ||
                !game->PeerTeleport(motion, output.destination, state.peerIdentity->character, nextSequence_, remote))
                return false;
            auto& live = state.activeWorld->character;
            live.x = output.destination.x;
            live.y = output.destination.y;
            live.z = output.destination.z;
            state.flightMotion.Clear(); // Wait for fresh client movement after the snap.
            BroadcastPeerLocked(state, std::move(remote));
            nextSequence_ += 2;
            result.push_back(std::move(teleport));
        }
    }
    for (const auto& line : output.lines) {
        std::vector<std::string> parts;
        if (!SplitChatText(line, game->TextLimits(), parts))
            return false;
        for (const auto& part : parts) {
            Buffer message;
            if (!game->ServiceChat(part, character.guid, message))
                return false;
            result.push_back(std::move(message));
        }
    }
    if (output.changeCombat && !CombatModeLocked(at->second, output.combatEnabled))
        return false;
    replies = std::move(result);
    command = std::move(output);
    return true;
}
bool Server::RelayChatLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                             const ChatMessage& chat, bool announcement) {
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld)
        return false;
    const auto game = at->second.gameMessages;
    if (!game)
        return false;
    if (!announcement && !game->IsMapChat(chat))
        return true;
    const auto& sender = at->second.activeWorld->character;
    ChatMessage routed = chat;
    if (announcement) {
        routed.text = "Announcement from " + sender.name + ": " + chat.text;
    }
    std::vector<std::string> parts;
    if (!SplitChatText(routed.text, game->TextLimits(), parts))
        return false;
    std::vector<std::shared_ptr<const Buffer>> encoded;
    size_t bytes = 0;
    for (const auto& text : parts) {
        ChatMessage part = routed;
        part.text = text;
        std::shared_ptr<Buffer> message(new Buffer);
        if (announcement) {
            if (!game->ServiceChat(part.text, sender.guid, *message))
                return false;
        } else if (!game->Chat(part, sender.name, sender.guid, *message))
            return false;
        bytes += message->Size();
        encoded.push_back(std::move(message));
    }
    for (auto& pair : sessions_) {
        auto& peer = pair.second;
        if (peer.loggedOut || !peer.worldOwner || !peer.worldReady || !peer.activeWorld || peer.outgoingOverflow ||
            (!announcement && peer.activeWorld->character.map != sender.map))
            continue;
        if (encoded.size() > 256 - peer.outgoing.size() || bytes > 256 * 1024 - peer.outgoingBytes) {
            peer.outgoingOverflow = true;
            peer.outgoing.clear();
            peer.outgoingBytes = 0;
            continue; // The owning worker closes this slow connection on its next drain.
        }
        peer.outgoing.insert(peer.outgoing.end(), encoded.begin(), encoded.end());
        peer.outgoingBytes += bytes;
    }
    return true;
}
bool Server::ProcessPendingTravel(const std::shared_ptr<const LoginSession>& session, uint64_t owner, Buffer& message,
                                  bool& transferred) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !profile_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || !at->second.activeWorld ||
        at->second.outgoingOverflow)
        return false;
    auto& state = at->second;
    const auto game = state.gameMessages;
    if (!game)
        return false;
    message.Clear();
    transferred = false;
    if (!state.requestedMap)
        return true;
    const unsigned map = state.requestedMap;
    const auto destination = state.requestedPosition;
    const auto character = state.activeWorld->character;
    state.requestedMap = 0; // Consume exactly once, including a failed save.
    WorldEntry entry;
    std::string error;
    if (TransferWorldLocked(session, owner, {character.x, character.y, character.z}, map, destination, entry, message,
                            error)) {
        transferred = true;
        return true;
    }
    QueueLogLocked("travel", error);
    return game->ServiceChat("The requested transfer could not be started. Your saved destination has not changed.",
                             character.guid, message);
}
bool Server::DrainWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                        std::vector<std::shared_ptr<const Buffer>>& messages) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.worldReady || at->second.outgoingOverflow)
        return false;
    if (RunWorldActionsLocked(at->second, ByteView(), true) == WorldActionResult::Failed)
        return false;
    if (RunWorldActionsLocked(at->second, ByteView(), false, true) == WorldActionResult::Failed)
        return false;
    if (!RefreshTauntLocked(at->second) || !RefreshRevealLocked(at->second) || !QueuePeerEffectsLocked(at->second))
        return false;
    messages.clear();
    messages.swap(at->second.outgoing);
    at->second.outgoingBytes = 0;
    return true;
}
} // namespace glide
