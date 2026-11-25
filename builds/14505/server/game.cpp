#include "game.h"
#include "game_request.h"
#include "game_reply.h"
#include "motion_state.h"
#include "world_environment.h"
#include "world_protocol.h"
#include "glide/profile/character_attitude.h"

namespace glide {
namespace june14505 {
namespace {
class JuneGameMessages final : public GameMessages {
  public:
    JuneGameMessages(uint16_t agent, unsigned build) : agent_(agent), build_(build) {}
    bool PeerAttitude(uint16_t subject, bool hostile, Buffer& out) const override {
        return CharacterAttitude(build_, subject, hostile, out);
    }
    bool TeleportNeedsMotion() const override {
        return false;
    }
    bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer& out) const override {
        return TimeOfDayForBuild(build_, ticks, rate, paused, out);
    }
    bool Signal(Buffer& out) const override {
        Buffer result;
        if (!result.U16(3))
            return false;
        out = std::move(result);
        return true;
    }
    bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const override {
        return MapTransferForBuild(build_, association, map, out);
    }
    PositionResult DecodePosition(ByteView input, CatalogPosition& out) const override {
        GameRequest request;
        if (!Request(input, request))
            return PositionResult::Invalid;
        return request.kind == GameRequestKind::Motion ? MotionPositionForBuild(input, build_, out)
                                                       : PositionResult::Absent;
    }
    FacingResult DecodeFacing(ByteView input, Facing& out) const override {
        GameRequest request;
        if (!Request(input, request))
            return FacingResult::Invalid;
        return request.kind == GameRequestKind::Motion ? MotionFacingForBuild(input, build_, out)
                                                       : FacingResult::Absent;
    }
    bool LocalTeleport(ByteView, const CatalogPosition& position, uint32_t sequence, Buffer& out) const override {
        return SyncPositionForBuild(build_, position, agent_, sequence, out);
    }
    bool PeerTeleport(ByteView, const CatalogPosition& position, uint16_t body, uint32_t sequence,
                      Buffer& out) const override {
        return SyncPositionForBuild(build_, position, body, sequence, out);
    }
    RelayResult PeerMotion(ByteView input, uint16_t body, uint32_t sequence, uint32_t, Buffer& out) const override {
        GameRequest request;
        if (!Request(input, request) || !body || !sequence)
            return RelayResult::Invalid;
        return request.kind == GameRequestKind::Motion ? RelayMotionForBuild(input, build_, body, sequence, out)
                                                       : RelayResult::Ignored;
    }
    RelayResult OwnMotion(ByteView input, uint32_t sequence, uint32_t tick, Buffer& out) const override {
        return PeerMotion(input, agent_, sequence, tick, out);
    }
    RelayResult PeerGlide(ByteView, ByteView, uint16_t, uint32_t, Buffer&) const override {
        return RelayResult::Ignored;
    }
    FlightUpdate DecodeFlight(ByteView input) const override {
        GameRequest request;
        return Request(input, request) ? FlightUpdate{} : FlightUpdate{FlightEvent::Invalid, 0};
    }
    bool Glider(uint32_t, uint16_t, Buffer&) const override {
        return false;
    }
    bool Mount(uint32_t, uint32_t, uint16_t, Buffer&) const override {
        return false;
    }
    bool MountStart(uint32_t, uint32_t, uint16_t, Buffer&) const override {
        return false;
    }
    bool Unmount(uint16_t, Buffer&) const override {
        return false;
    }
    bool DecodeWaypoint(ByteView input, uint32_t& out) const override {
        GameRequest request;
        if (!Request(input, request) || request.kind != GameRequestKind::Waypoint)
            return false;
        out = request.value;
        return true;
    }
    bool DecodeChat(ByteView input, ChatMessage& out) const override {
        GameRequest request;
        if (!Request(input, request) ||
            (request.kind != GameRequestKind::ChatChannel && request.kind != GameRequestKind::ChatTarget &&
             request.kind != GameRequestKind::ServerCommand))
            return false;
        ChatMessage message;
        message.text = std::move(request.text);
        message.channel = request.channel;
        message.target = request.value;
        message.targeted = request.kind == GameRequestKind::ChatTarget;
        if (request.kind == GameRequestKind::ServerCommand) {
            const size_t prefix = message.text.compare(0, 5, "/map ") == 0 ? 5
                                  : message.text.compare(0, 3, "/m ") == 0 ? 3
                                                                           : 0;
            if (prefix && message.text.find_first_not_of(" \t", prefix) != std::string::npos) {
                message.text.erase(0, prefix);
                message.channel = 9;
            }
            // Other native server commands stay non-public instead of becoming map chat.
        }
        out = std::move(message);
        return true;
    }
    bool IsMapChat(const ChatMessage& message) const override {
        return !message.targeted && (message.channel == 8 || message.channel == 9);
    }
    ChatTextLimits TextLimits() const override {
        return {251, true};
    }
    bool ServiceChat(const std::string& text, const Guid&, Buffer& out) const override {
        return ChatReplyForBuild(build_, text, out);
    }
    bool Chat(const ChatMessage& message, const std::string& sender, const Guid& guid, Buffer& out) const override {
        if (!IsMapChat(message) || message.flags)
            return false;
        return ChatPublicationForBuild(build_, message.text, sender, guid, message.channel, 0x11, out);
    }

  private:
    bool Request(ByteView input, GameRequest& out) const {
        if (DecodeGameRequestForBuild(input, build_, out) != DecodeStatus::Complete || out.consumed != input.size)
            return false;
        return out.kind != GameRequestKind::Motion ||
               (out.motion.canonicalId == 0 ||
                (agent_ && (out.motion.canonicalId == 0x000d || out.motion.agent == agent_)));
    }
    uint16_t agent_;
    unsigned build_;
};
} // namespace
std::unique_ptr<GameMessages> MakeGameMessagesForBuild(unsigned build, uint16_t ownedAgent) {
    if (!WorldProtocolBuild(build) || !ownedAgent || (build == 14075 && (ownedAgent < 257 || ownedAgent > 510)))
        return {};
    return std::unique_ptr<GameMessages>(new JuneGameMessages(ownedAgent, build));
}
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent) {
    return MakeGameMessagesForBuild(14505, ownedAgent);
}
const GameMessages* ServiceMessagesForBuild(unsigned build) {
    static JuneGameMessages april(0, 14075), june7(0, 14422), june11(0, 14505);
    return build == 14075 ? &april : build == 14422 ? &june7 : build == 14505 ? &june11 : nullptr;
}
const GameMessages& ServiceMessages() {
    return *ServiceMessagesForBuild(14505);
}
} // namespace june14505
} // namespace glide
