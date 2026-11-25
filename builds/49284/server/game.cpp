#include "game.h"
#include "game_wire.h"
#include "motion.h"
#include "motion_state.h"
#include "chat.h"
#include "entry_steps.h"
#include "game_initializer.h"
#include "glide/profile/character_attitude.h"
namespace glide {
namespace hot49284 {
namespace {
class HotGame final : public GameMessages {
  public:
    HotGame(uint16_t agent, uint16_t port) : agent_(agent), port_(port) {}
    bool PeerAttitude(uint16_t subject, bool hostile, Buffer& out) const override {
        return CharacterAttitude(49284, subject, hostile, out);
    }
    bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out) const override {
        return hot49284::TimeOfDay(tick, rate, paused, out);
    }
    bool Signal(Buffer& out) const override {
        Buffer value;
        if (!value.U16(3))
            return false;
        out = std::move(value);
        return true;
    }
    bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const override {
        return map && map <= 65535 && Initializers().Redirect(association, static_cast<uint16_t>(map), port_, out);
    }
    bool TeleportNeedsMotion() const override {
        return false;
    }
    bool LocalTeleport(ByteView, const CatalogPosition& point, uint32_t sequence, Buffer& out) const override {
        return SyncPosition(point, agent_, sequence, out);
    }
    bool PeerTeleport(ByteView, const CatalogPosition& point, uint16_t body, uint32_t sequence,
                      Buffer& out) const override {
        return SyncPosition(point, body, sequence, out);
    }
    PositionResult DecodePosition(ByteView input, CatalogPosition& out) const override {
        return Valid(input) ? MotionPosition(input, out) : PositionResult::Invalid;
    }
    FacingResult DecodeFacing(ByteView input, Facing& out) const override {
        return Valid(input) ? MotionFacing(input, out) : FacingResult::Invalid;
    }
    RelayResult OwnMotion(ByteView input, uint32_t sequence, uint32_t, Buffer& out) const override {
        return Valid(input) ? hot49284::OwnMotion(input, agent_, sequence, out) : RelayResult::Invalid;
    }
    RelayResult PeerMotion(ByteView input, uint16_t body, uint32_t sequence, uint32_t, Buffer& out) const override {
        if (!Valid(input) || !body || !sequence)
            return RelayResult::Invalid;
        MotionMessage message{};
        size_t consumed = 0;
        const auto status = DecodeMotion(input, message, consumed);
        if (status == DecodeStatus::Unknown)
            return RelayResult::Ignored;
        if (status != DecodeStatus::Complete)
            return RelayResult::Invalid;
        if (message.id == 5 || message.id == 15 || message.id == 16)
            return RelayResult::Ignored;
        return RelayMotion(input, body, sequence, out) ? RelayResult::Encoded : RelayResult::Invalid;
    }
    RelayResult PeerGlide(ByteView, ByteView, uint16_t, uint32_t, Buffer&) const override {
        return RelayResult::Ignored;
    }
    bool NeedsActionTickBeforeMotion(ByteView input) const override {
        MotionMessage m{};
        size_t consumed = 0;
        return Valid(input) && DecodeMotion(input, m, consumed) == DecodeStatus::Complete && consumed == input.size &&
               m.id == 9 && m.state == 3 && m.input_size == 3 && m.input[0] == 0x51 && m.input[1] == 0 &&
               m.input[2] == 0;
    }
    FlightUpdate DecodeFlight(ByteView input) const override {
        return Valid(input) ? FlightUpdate{} : FlightUpdate{FlightEvent::Invalid, 0};
    }
    bool Glider(uint32_t design, uint16_t peerBody, Buffer& out) const override {
        if (design != 1)
            return false;
        const uint16_t body = peerBody ? peerBody : agent_;
        if (body < 257 || body > 510)
            return false;
        Buffer result;
        if (!result.U16(172) || !result.VarU32(body) || !result.U16(1))
            return false;
        if (!peerBody && (!result.U16(174) || !result.U16(1)))
            return false;
        out = std::move(result);
        return true;
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
        ChatMessage message;
        size_t consumed = 0;
        if (hot49284::DecodeChat(input, message, consumed) != DecodeStatus::Complete || consumed != input.size)
            return false;
        out = std::move(message);
        return true;
    }
    // Native Say25 and Map26 requests verified with the original client.
    bool IsMapChat(const ChatMessage& message) const override {
        return hot49284::IsPublicChat(message);
    }
    bool Chat(const ChatMessage& message, const std::string& sender, const Guid& identity, Buffer& out) const override {
        return hot49284::PlayerChat(message, sender, identity, out);
    }
    ChatTextLimits TextLimits() const override {
        return {251, true};
    }
    bool ServiceChat(const std::string& text, const Guid&, Buffer& out) const override {
        return hot49284::ServiceChat(text, out);
    }

  private:
    bool Valid(ByteView input) const {
        GameWire wire;
        GameMessage message;
        if (wire.DecodeGame(input, message) != DecodeStatus::Complete || message.consumed != input.size)
            return false;
        if (message.action != GameAction::Motion && message.action != GameAction::Position)
            return true;
        MotionMessage motion{};
        size_t consumed = 0;
        return agent_ && DecodeMotion(input, motion, consumed) == DecodeStatus::Complete &&
               (motion.id == 15 || motion.id == 16 || motion.agent == agent_);
    }
    uint16_t agent_, port_;
};
} // namespace
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t agent, uint16_t port) {
    if (agent < 257 || agent > 510 || !port)
        return {};
    return std::unique_ptr<GameMessages>(new HotGame(agent, port));
}
const GameMessages& ServiceMessages() {
    static HotGame messages(0, 0);
    return messages;
}
} // namespace hot49284
} // namespace glide
