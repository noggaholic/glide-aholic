#include "game.h"
#include "game_wire.h"
#include "motion.h"
#include "motion_state.h"
#include "chat.h"
#include "bootstrap.h"
#include "game_initializer.h"
#include "glide/profile/character_attitude.h"
namespace glide {
namespace july14812 {
namespace {
class JulyGame final : public GameMessages {
  public:
    JulyGame(uint16_t agent, uint16_t port) : agent_(agent), port_(port) {}
    bool PeerAttitude(uint16_t subject, bool hostile, Buffer& out) const override {
        return CharacterAttitude(14812, subject, hostile, out);
    }
    bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out) const override {
        return july14812::TimeOfDay(tick, rate, paused, out);
    }
    bool Signal(Buffer& out) const override {
        Buffer result;
        if (!result.U16(3))
            return false;
        out = std::move(result);
        return true;
    }
    bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const override {
        return map && map <= UINT16_MAX && Initializers().Redirect(association, static_cast<uint16_t>(map), port_, out);
    }
    bool TeleportNeedsMotion() const override {
        return false;
    }
    bool LocalTeleport(ByteView, const CatalogPosition& point, uint32_t sequence, Buffer& out) const override {
        return SyncPosition(point, agent_, sequence, out);
    }
    bool PeerTeleport(ByteView, const CatalogPosition& point, uint16_t agent, uint32_t sequence,
                      Buffer& out) const override {
        return SyncPosition(point, agent, sequence, out);
    }
    PositionResult DecodePosition(ByteView input, CatalogPosition& out) const override {
        return Valid(input) ? MotionPosition(input, out) : PositionResult::Invalid;
    }
    FacingResult DecodeFacing(ByteView input, Facing& out) const override {
        return Valid(input) ? MotionFacing(input, out) : FacingResult::Invalid;
    }
    RelayResult OwnMotion(ByteView input, uint32_t sequence, uint32_t, Buffer& out) const override {
        return Valid(input) ? july14812::OwnMotion(input, agent_, sequence, out) : RelayResult::Invalid;
    }
    RelayResult PeerMotion(ByteView input, uint16_t peer, uint32_t sequence, uint32_t, Buffer& out) const override {
        if (!Valid(input) || !peer || !sequence)
            return RelayResult::Invalid;
        MotionMessage m;
        size_t consumed = 0;
        const auto status = DecodeMotion(input, m, consumed);
        if (status == DecodeStatus::Unknown)
            return RelayResult::Ignored;
        if (status != DecodeStatus::Complete)
            return RelayResult::Invalid;
        if (m.id == 12 || m.id == 13)
            return RelayResult::Ignored;
        return EncodeMotion(m, peer, sequence, out) ? RelayResult::Encoded : RelayResult::Invalid;
    }
    RelayResult PeerGlide(ByteView, ByteView, uint16_t, uint32_t, Buffer&) const override {
        return RelayResult::Ignored;
    }
    FlightUpdate DecodeFlight(ByteView input) const override {
        return Valid(input) ? FlightUpdate{} : FlightUpdate{FlightEvent::Invalid, 0};
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
    bool DecodeChat(ByteView input, ChatMessage& out) const override {
        ChatMessage value;
        size_t consumed = 0;
        if (july14812::DecodeChat(input, value, consumed) != DecodeStatus::Complete || consumed != input.size)
            return false;
        out = std::move(value);
        return true;
    }
    bool IsMapChat(const ChatMessage& message) const override {
        return july14812::IsPublicChat(message);
    }
    bool Chat(const ChatMessage& message, const std::string& sender, const Guid& identity, Buffer& out) const override {
        return july14812::PublishChat(message, sender, identity, out);
    }
    ChatTextLimits TextLimits() const override {
        return {251, true};
    }
    bool ServiceChat(const std::string& text, const Guid&, Buffer& out) const override {
        return july14812::ServiceChat(text, out);
    }

  private:
    bool Valid(ByteView input) const {
        GameWire wire;
        GameMessage message;
        if (wire.DecodeGame(input, message) != DecodeStatus::Complete || message.consumed != input.size)
            return false;
        return (message.wireId < 3 || message.wireId > 16) || ValidOwnedMotion(input, agent_);
    }
    uint16_t agent_, port_;
};
} // namespace
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t agent, uint16_t port) {
    if (agent < 257 || agent > 510 || !port)
        return {};
    return std::unique_ptr<GameMessages>(new JulyGame(agent, port));
}
const GameMessages& ServiceMessages() {
    static JulyGame messages(0, 0);
    return messages;
}
} // namespace july14812
} // namespace glide
