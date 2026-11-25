#include "server.h"
#include "commands.h"
#include "glide/bootstrap_runner.h"
#include <cstdio>
#include <algorithm>

namespace glide {
void RunGame(Server& server, Socket& socket, MsgConnStream& connection,
             const std::shared_ptr<const LoginSession>& session, uint64_t owner) {
    const auto& profile = *server.Profile();
    std::vector<BootstrapStep> steps;
    std::string error;
    Buffer input, entryWarning;
    std::unique_ptr<WireMap> sessionWire;
    if (!profile.CreateGameWire(session->login, sessionWire, error)) {
        server.Logger().Write("game", error);
        return;
    }
    const WireMap& wire = sessionWire ? *sessionWire : profile.Wire();
    if (!server.PrepareWorld(session, owner, steps, error)) {
        server.Logger().Write("game", error);
        return;
    }
    const auto messages = server.WorldMessages(session, owner);
    if (!messages)
        return;
    if (server.Verbose())
        server.Logger().Write("game", "World loading steps=" + std::to_string(steps.size()));
    const auto loadingControl = [&](const GameMessage& message) {
        if (message.action != GameAction::HintCompleted)
            return true;
        if (server.SaveCompletedHint(session, owner, message.hint, error))
            return true;
        server.Logger().Write("tutorial", error);
        return false;
    };
    if (!RunBootstrap(socket, connection, wire, steps, profile.Transport().serverFrames, input, server.Logger(),
                      loadingControl, server.Verbose()) ||
        !server.WorldReady(session, owner, &entryWarning))
        return;
    if (entryWarning.Size() && !connection.Send(entryWarning.View(), profile.Transport().serverFrames))
        return;
    server.Logger().Write("game", "Character entered world");
    uint64_t nextSignal = GetTickCount64() + 10000;
    CatalogPosition position;
    bool dirty = false, active = true;
    Facing facing;
    bool hasFacing = false;
    Buffer motion;
    unsigned motionSamples = 0, positionSamples = 0, chatSamples = 0;
    while (active && !socket.Stopped()) {
        Buffer requestedTravel;
        bool transferred = false;
        if (!server.ProcessPendingTravel(session, owner, requestedTravel, transferred))
            break;
        if (transferred)
            dirty = false; // Transfer saved the current origin before releasing this world.
        if (requestedTravel.Size() && !connection.Send(requestedTravel.View(), profile.Transport().serverFrames))
            break;
        if (transferred)
            break;
        size_t consumed = 0, processed = 0;
        for (; processed < 256; ++processed) {
            GameMessage message;
            const ByteView available(input.Data() ? input.Data() + consumed : nullptr, input.Size() - consumed);
            const auto status = wire.DecodeGame(available, message);
            if (status == DecodeStatus::Incomplete)
                break;
            if (status != DecodeStatus::Complete || !message.consumed || message.consumed > available.size) {
                Reader head(available);
                uint16_t id = 0;
                head.U16(id);
                char diagnostic[128];
                std::snprintf(diagnostic, sizeof(diagnostic), "%s message id=0x%04x buffered=%u; connection closed",
                              status == DecodeStatus::Unknown ? "Unknown" : "Malformed", static_cast<unsigned>(id),
                              static_cast<unsigned>(available.size));
                server.Logger().Write("game", diagnostic);
                active = false;
                break;
            }
            if (!wire.AllowsInWorld(message)) {
                server.Logger().Write("game", "Message invalid for current world state");
                active = false;
                break;
            }
            if (message.action == GameAction::Close) {
                consumed += message.consumed;
                server.Logger().Write("game", "Client requested world close");
                active = false;
                break;
            }
            if (message.action == GameAction::HintCompleted) {
                if (!server.SaveCompletedHint(session, owner, message.hint, error)) {
                    server.Logger().Write("tutorial", error);
                    active = false;
                    break;
                }
            } else if (message.action == GameAction::Inventory) {
                if (!server.ApplyWorldAction(session, owner, ByteView(available.data, message.consumed))) {
                    active = false;
                    break;
                }
            } else if (message.action == GameAction::Motion || message.action == GameAction::Position) {
                // Opt-in bounded samples: only movement, never account or chat payloads.
                const bool positionReport = message.action == GameAction::Position;
                unsigned& sampleCount = positionReport ? positionSamples : motionSamples;
                if (server.Verbose() && sampleCount < (positionReport ? 8u : 64u)) {
                    char header[128];
                    std::snprintf(header, sizeof(header), "owner=%llu id=0x%04x bytes=%u sample=%u hex=",
                                  static_cast<unsigned long long>(owner), static_cast<unsigned>(message.wireId),
                                  static_cast<unsigned>(message.consumed), ++sampleCount);
                    std::string diagnostic(header);
                    static const char digits[] = "0123456789abcdef";
                    const size_t count = (std::min)(message.consumed, size_t(256));
                    for (size_t i = 0; i < count; ++i) {
                        diagnostic.push_back(digits[available.data[i] >> 4]);
                        diagnostic.push_back(digits[available.data[i] & 15]);
                    }
                    if (count < message.consumed)
                        diagnostic += "...";
                    server.Logger().Write("motion", diagnostic);
                }
                CatalogPosition next;
                const auto result = messages->DecodePosition(ByteView(available.data, message.consumed), next);
                if (result == PositionResult::Invalid) {
                    server.Logger().Write("game", "Invalid position; connection closed");
                    active = false;
                    break;
                }
                Facing direction;
                const auto facingResult = messages->DecodeFacing(ByteView(available.data, message.consumed), direction);
                if (facingResult == FacingResult::Invalid) {
                    server.Logger().Write("game", "Invalid facing; connection closed");
                    active = false;
                    break;
                }
                if (facingResult == FacingResult::Present) {
                    facing = direction;
                    hasFacing = true;
                    motion.Clear();
                    if (!motion.Append(ByteView(available.data, message.consumed))) {
                        active = false;
                        break;
                    }
                }
                if (result == PositionResult::Present) {
                    position = next;
                    dirty = true;
                    if (!server.UpdateWorldPosition(session, owner, position, &position)) {
                        active = false;
                        break;
                    }
                }
                if (!server.RelayWorldMotion(session, owner, ByteView(available.data, message.consumed))) {
                    active = false;
                    break;
                }
            }
            if (message.action == GameAction::Chat || message.action == GameAction::Waypoint) {
                std::vector<Buffer> replies;
                CommandResult command;
                const ByteView request(available.data, message.consumed);
                if (message.action == GameAction::Waypoint) {
                    uint32_t waypoint = 0;
                    // The wire reader has already validated this message. Profiles
                    // without a waypoint decoder retain their existing ignored behavior.
                    if (!messages->DecodeWaypoint(request, waypoint)) {
                        consumed += message.consumed;
                        continue;
                    }
                    if (!server.WaypointReplies(session, owner, waypoint, motion.View(), replies, command)) {
                        active = false;
                        break;
                    }
                } else {
                    ChatMessage chat;
                    if (!messages->DecodeChat(request, chat)) {
                        active = false;
                        break;
                    }
                    if (server.Verbose() && chatSamples++ < 16)
                        server.Logger().Write("chat", "Decoded channel=" + std::to_string(chat.channel) +
                                                          " targeted=" + std::to_string(chat.targeted ? 1 : 0));
                    if (!server.CommandReplies(session, owner, chat, dirty ? &position : nullptr,
                                               hasFacing ? &facing : nullptr, motion.View(), replies, command)) {
                        active = false;
                        break;
                    }
                }
                if (command.transferMap)
                    dirty = false; // The origin was saved before issuing the redirect.
                for (const auto& reply : replies)
                    if (!connection.Send(reply.View(), profile.Transport().serverFrames)) {
                        active = false;
                        break;
                    }
                if (!active)
                    break;
                if (command.transferMap) {
                    active = false;
                    break;
                }
                if (command.teleport) {
                    position = command.destination;
                    dirty = true;
                    if (!server.UpdateWorldPosition(session, owner, position, &position)) {
                        active = false;
                        break;
                    }
                    if (!server.SaveWorldPosition(session, owner, position, error)) {
                        server.Logger().Write("game", error);
                        Guid sender;
                        Buffer reply;
                        if (!messages->ServiceChat("The position could not be saved.", sender, reply) ||
                            !connection.Send(reply.View(), profile.Transport().serverFrames)) {
                            active = false;
                            break;
                        }
                    }
                }
            }
            consumed += message.consumed;
        }
        input.Consume(consumed);
        if (!active)
            break;
        std::vector<std::shared_ptr<const Buffer>> outgoing;
        if (!server.DrainWorld(session, owner, outgoing))
            break;
        for (const auto& message : outgoing) {
            const bool sent = connection.Send(message->View(), profile.Transport().serverFrames);
            if (server.Verbose())
                server.TraceWorldDelivery(session, owner, message, sent);
            if (!sent) {
                active = false;
                break;
            }
        }
        if (!active)
            break;
        if (GetTickCount64() >= nextSignal) {
            Buffer signal;
            if (!messages->Signal(signal) || !connection.Send(signal.View(), profile.Transport().serverFrames))
                break;
            nextSignal = GetTickCount64() + 10000;
        }
        if (processed == 256)
            continue;
        if (connection.HasBufferedInput() || socket.WaitReadable(100)) {
            uint8_t bytes[8192];
            size_t received = 0;
            if (!connection.Read(bytes, sizeof(bytes), received) || received > 2 * 1024 * 1024 - input.Size() ||
                !input.Append(ByteView(bytes, received)))
                break;
        }
    }
    if (dirty && !server.SaveWorldPosition(session, owner, position, error))
        server.Logger().Write("game", error);
    server.Logger().Write("game", "World connection closed");
}
} // namespace glide
