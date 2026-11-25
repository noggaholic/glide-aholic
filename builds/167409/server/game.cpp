#include "game.h"
#include "protocol.h"
#include "glide/files.h"
#include "glide/coded_text.h"
#include "glide/profile/endurance.h"
#include "glide/profile/character_attitude.h"

namespace glide {
const GameMessages& Build204489Game();
namespace {
bool Complete(ByteView input, GameMessage& message) {
    return China167409Wire().DecodeGame(input, message) == DecodeStatus::Complete && message.consumed == input.size;
}
template <class Encode> bool Regional(Encode encode, Buffer& out) {
    Buffer result;
    if (!encode(result) || !china167409::RemapServerMessage(result))
        return false;
    out = std::move(result);
    return true;
}
class ChinaGame final : public GameMessages {
  public:
    bool PeerAttitude(uint16_t subject, bool hostile, Buffer& out) const override {
        return CharacterAttitude(167409, subject, hostile, out);
    }
    bool NeedsActionTickBeforeMotion(ByteView input) const override {
        uint32_t token = 0;
        GameMessage decoded;
        return modern::ModernDodgeToken(input, token) && Complete(input, decoded);
    }
    ChatTextLimits TextLimits() const override {
        return {251, true};
    }
    bool ServiceChat(const std::string& text, const Guid& identity, Buffer& out) const override {
        ChatMessage message;
        message.text = text;
        message.channel = 0x23;
        message.target = UINT32_MAX;
        return Chat(message, "Glide Aholic", identity, out);
    }
    bool IsMapChat(const ChatMessage& message) const override {
        return !message.targeted && !message.flags && (message.target == 0 || message.target == UINT32_MAX) &&
               (message.channel == 0x23 || message.channel == 0x24);
    }
    bool DecodeChat(ByteView input, ChatMessage& out) const override {
        GameMessage decoded;
        if (!Complete(input, decoded) || decoded.action != GameAction::Chat)
            return false;
        Buffer common;
        if (!common.U16(0x0106) || !common.Append(ByteView(input.data + 2, input.size - 2)))
            return false;
        ChatMessage result;
        if (!Build204489Game().DecodeChat(common.View(), result))
            return false;
        result.targeted = result.target != 0 && result.target != UINT32_MAX;
        out = std::move(result);
        return true;
    }
    bool Chat(const ChatMessage& message, const std::string& sender, const Guid& identity, Buffer& out) const override {
        std::wstring text, name;
        if (message.text.size() > 1004 || sender.size() > 284 || message.text.find('\0') != std::string::npos ||
            sender.find('\0') != std::string::npos || !Utf8ToWide(message.text, text) || text.size() > 251 ||
            !Utf8ToWide(sender, name) || name.size() > 71)
            return false;
        bool anonymous = true;
        for (auto byte : identity.bytes)
            if (byte) {
                anonymous = false;
                break;
            }
        Buffer result;
        if (!result.U16(0x02b6) || !AppendCodedText(text, 256, result) || !result.U16(0x02b7) ||
            !result.U8(message.channel) || !result.U8(0x16))
            return false;
        // The original handler treats zero-GUID names as coded text, while
        // player names with a GUID remain raw text for the linked-user path.
        if (!(anonymous ? AppendCodedText(name, 72, result) : result.Utf16Z(sender.c_str(), 71)))
            return false;
        if (!result.Append(ByteView(identity.bytes, 16)) || !result.VarU32(message.target) || !result.VarU32(0) ||
            !result.U8(message.flags))
            return false;
        out = std::move(result);
        return true;
    }
    bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer& out) const override {
        return Regional([&](Buffer& b) { return Build204489Game().TimeOfDay(ticks, rate, paused, b); }, out);
    }
    bool Glider(uint32_t design, uint16_t body, Buffer& out) const override {
        return Regional([&](Buffer& b) { return Build204489Game().Glider(design, body, b); }, out);
    }
    bool Mount(uint32_t skin, uint32_t definition, uint16_t body, Buffer& out) const override {
        return Regional([&](Buffer& b) { return Build204489Game().Mount(skin, definition, body, b); }, out);
    }
    bool Unmount(uint16_t body, Buffer& out) const override {
        return Regional([&](Buffer& b) { return Build204489Game().Unmount(body, b); }, out);
    }
    bool MountStart(uint32_t skin, uint32_t definition, uint16_t body, Buffer& out) const override {
        Buffer result;
        if (!Build204489Game().MountStart(skin, definition, body, result) || result.Size() < 2)
            return false;
        // Own group49 mount transition descriptor, same payload as modern 0x04be.
        result.Data()[0] = 0x9d;
        result.Data()[1] = 0x04;
        out = std::move(result);
        return true;
    }
    bool Signal(Buffer& out) const override {
        return Build204489Game().Signal(out);
    }
    bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const override {
        return Regional([&](Buffer& b) { return Build204489Game().MapRedirect(association, map, b); }, out);
    }
    PositionResult DecodePosition(ByteView input, CatalogPosition& out) const override {
        GameMessage message;
        if (!Complete(input, message))
            return PositionResult::Invalid;
        if (message.action != GameAction::Position && message.action != GameAction::Motion)
            return PositionResult::Absent;
        return Build204489Game().DecodePosition(input, out);
    }
    FacingResult DecodeFacing(ByteView input, Facing& out) const override {
        GameMessage message;
        if (!Complete(input, message))
            return FacingResult::Invalid;
        if (message.action != GameAction::Position && message.action != GameAction::Motion)
            return FacingResult::Absent;
        return Build204489Game().DecodeFacing(input, out);
    }
    FlightUpdate DecodeFlight(ByteView input) const override {
        GameMessage message;
        if (!Complete(input, message))
            return {FlightEvent::Invalid, 0};
        if (message.action != GameAction::Position && message.action != GameAction::Motion)
            return {};
        return Build204489Game().DecodeFlight(input);
    }
    bool LocalTeleport(ByteView input, const CatalogPosition& position, uint32_t sequence, Buffer& out) const override {
        GameMessage message;
        return Complete(input, message) &&
               Regional([&](Buffer& b) { return Build204489Game().LocalTeleport(input, position, sequence, b); }, out);
    }
    bool PeerTeleport(ByteView input, const CatalogPosition& position, uint16_t body, uint32_t sequence,
                      Buffer& out) const override {
        GameMessage message;
        return Complete(input, message) &&
               Regional([&](Buffer& b) { return Build204489Game().PeerTeleport(input, position, body, sequence, b); },
                        out);
    }
    RelayResult PeerMotion(ByteView input, uint16_t body, uint32_t sequence, uint32_t tick,
                           Buffer& out) const override {
        GameMessage message;
        if (!Complete(input, message))
            return RelayResult::Invalid;
        Buffer result;
        const auto status = Build204489Game().PeerMotion(input, body, sequence, tick, result);
        if (status != RelayResult::Encoded)
            return status;
        if (!china167409::RemapServerMessage(result))
            return RelayResult::Invalid;
        out = std::move(result);
        return status;
    }
    RelayResult PeerGlide(ByteView motion, ByteView position, uint16_t body, uint32_t sequence,
                          Buffer& out) const override {
        GameMessage decoded;
        if (!Complete(motion, decoded) || !Complete(position, decoded))
            return RelayResult::Invalid;
        Buffer result;
        const auto status = Build204489Game().PeerGlide(motion, position, body, sequence, result);
        if (status != RelayResult::Encoded)
            return status;
        if (!china167409::RemapServerMessage(result))
            return RelayResult::Invalid;
        out = std::move(result);
        return status;
    }
};
} // namespace
const GameMessages& China167409Game() {
    static const ChinaGame game;
    return game;
}
} // namespace glide
