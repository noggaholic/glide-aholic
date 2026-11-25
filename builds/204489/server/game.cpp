#include "glide/profile/game_messages.h"
#include "glide/profile/character_attitude.h"
#include "glide/profile/wire_map.h"
#include "glide/files.h"
#include "glide/coded_text.h"
#include "glide/profile/endurance.h"
#include <cmath>
#include <cstring>

namespace glide {
const WireMap& Build204489Wire();
namespace {
class Build204489GameMessages : public GameMessages {
  public:
    bool PeerAttitude(uint16_t subject, bool hostile, Buffer& out) const override {
        return CharacterAttitude(204489, subject, hostile, out);
    }
    bool NeedsActionTickBeforeMotion(ByteView input) const override {
        uint32_t token = 0;
        GameMessage decoded;
        return modern::ModernDodgeToken(input, token) &&
               Build204489Wire().DecodeGame(input, decoded) == DecodeStatus::Complete && decoded.consumed == input.size;
    }
    bool DecodeWaypoint(ByteView input, uint32_t& out) const override {
        GameMessage decoded;
        if (Build204489Wire().DecodeGame(input, decoded) != DecodeStatus::Complete || decoded.consumed != input.size ||
            decoded.action != GameAction::Waypoint)
            return false;
        Reader reader(input);
        uint16_t id;
        uint32_t point;
        if (!reader.U16(id) || reader.VarU32(point) != DecodeStatus::Complete || reader.Remaining())
            return false;
        out = point;
        return true;
    }
    ChatTextLimits TextLimits() const override {
        return {255, false};
    }
    bool ServiceChat(const std::string& text, const Guid& context, Buffer& out) const override {
        ChatMessage message;
        message.text = text;
        message.channel = 0x23;
        message.target = UINT32_MAX;
        return Chat(message, "Glide Aholic", context, out);
    }
    bool IsMapChat(const ChatMessage& message) const override {
        return !message.targeted && (message.channel == 0x23 || message.channel == 0x24);
    }
    bool Glider(uint32_t design, uint16_t peerBody, Buffer& out) const override {
        if (!design || design > UINT16_MAX || peerBody == 57344)
            return false;
        Buffer message;
        if (!message.U16(0x00f6) || !message.VarU32(peerBody ? peerBody : 57344) ||
            !message.U16(static_cast<uint16_t>(design)))
            return false;
        out = std::move(message);
        return true;
    }
    bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer& out) const override {
        if (ticks >= 7200000 || !std::isfinite(rate) || rate < 0)
            return false;
        Buffer message;
        if (!message.U16(0x04dd) || !message.VarU32(ticks) || !message.F32(rate) || !message.U8(paused ? 1 : 0))
            return false;
        out = std::move(message);
        return true;
    }
    bool Mount(uint32_t skin, uint32_t definition, uint16_t peerBody, Buffer& out) const override {
        if (!skin || !definition || peerBody == 57344)
            return false;
        const uint16_t body = peerBody ? peerBody : 57344;
        Buffer message;
        if (!message.U16(0x04bc) || !message.VarU32(body) || !message.U16(0) || !message.VarU32(body) ||
            !message.U16(1) || !message.VarU32(skin) || !message.VarU32(definition) || !message.VarU32(0) ||
            !message.VarU32(3) || !message.VarU32(1))
            return false;
        out = std::move(message);
        return true;
    }
    bool Unmount(uint16_t peerBody, Buffer& out) const override {
        if (peerBody == 57344)
            return false;
        Buffer message;
        if (!message.U16(0x04bd) || !message.VarU32(peerBody ? peerBody : 57344))
            return false;
        out = std::move(message);
        return true;
    }
    bool MountStart(uint32_t skin, uint32_t definition, uint16_t peerBody, Buffer& out) const override {
        if (!skin || !definition || peerBody == 57344)
            return false;
        const uint16_t body = peerBody ? peerBody : 57344;
        Buffer message;
        // Riding transition: seat 0, transition flag 1, rider flags 3, mount flags 1.
        if (!message.U16(0x04be) || !message.VarU32(body) || !message.U16(0) || !message.VarU32(body) ||
            !message.U16(1) || !message.VarU32(skin) || !message.VarU32(definition) || !message.VarU32(0) ||
            !message.VarU32(1) || !message.VarU32(3) || !message.VarU32(1))
            return false;
        out = std::move(message);
        return true;
    }
    FlightUpdate DecodeFlight(ByteView motion) const override {
        GameMessage decoded;
        if (Build204489Wire().DecodeGame(motion, decoded) != DecodeStatus::Complete || decoded.consumed != motion.size)
            return {FlightEvent::Invalid, 0};
        if (decoded.wireId == 4 || decoded.wireId == 0x1a)
            return {FlightEvent::Stop, 0};
        if (decoded.wireId != 0xb && decoded.wireId != 0x12 && decoded.wireId != 0x17)
            return {};
        Reader reader(motion);
        uint16_t word;
        uint32_t sequence, ignored;
        uint8_t flags, size;
        ByteView blob;
        if (!reader.U16(word) || (decoded.wireId != 0x12 && !reader.U16(word)) ||
            reader.VarU32(sequence) != DecodeStatus::Complete)
            return {FlightEvent::Invalid, 0};
        if (decoded.wireId == 0x12)
            return {FlightEvent::Position, static_cast<uint16_t>(sequence)};
        if (decoded.wireId == 0xb)
            return {FlightEvent::DetailedMotion, static_cast<uint16_t>(sequence)};
        if (!reader.U8(flags) || reader.VarU32(ignored) != DecodeStatus::Complete || !reader.U8(size) ||
            !reader.Bytes(size, blob))
            return {FlightEvent::Invalid, 0};
        return {size == 18 && blob.data[0] == 0xc && blob.data[1] == 2 ? FlightEvent::Deploy : FlightEvent::None, 0};
    }
    bool LocalTeleport(ByteView motion, const CatalogPosition& position, uint32_t sequence,
                       Buffer& out) const override {
        return Teleport(motion, position, 57344, sequence, out);
    }
    bool PeerTeleport(ByteView motion, const CatalogPosition& position, uint16_t body, uint32_t sequence,
                      Buffer& out) const override {
        return body && body != 57344 && Teleport(motion, position, body, sequence, out);
    }
    RelayResult PeerMotion(ByteView motion, uint16_t body, uint32_t sequence, uint32_t actionTick,
                           Buffer& out) const override {
        GameMessage decoded;
        CatalogPosition position;
        Facing facing;
        if (!body || body == 57344 || !sequence || sequence == UINT32_MAX ||
            Build204489Wire().DecodeGame(motion, decoded) != DecodeStatus::Complete ||
            decoded.consumed != motion.size || DecodePosition(motion, position) == PositionResult::Invalid ||
            DecodeFacing(motion, facing) == FacingResult::Invalid)
            return RelayResult::Invalid;
        uint16_t serverId = 0;
        switch (decoded.wireId) {
        case 0x0004:
            serverId = 0x0021;
            break;
        case 0x0006:
            serverId = 0x0022;
            break;
        case 0x0007:
            serverId = 0x0023;
            break;
        case 0x0009:
            serverId = 0x0034;
            break;
        case 0x000a:
            serverId = 0x0036;
            break;
        case 0x000b:
            serverId = 0x0039;
            break;
        case 0x000d:
            serverId = 0x003e;
            break;
        case 0x000e:
            serverId = 0x003f;
            break;
        case 0x000f:
            serverId = 0x0042;
            break;
        case 0x0010:
            serverId = 0x0047;
            break;
        case 0x0014:
            serverId = 0x004f;
            break;
        case 0x0017:
            serverId = 0x0054;
            break;
        case 0x0018:
            serverId = 0x0055;
            break;
        case 0x0019:
            serverId = 0x0056;
            break;
        case 0x001a:
            serverId = 0x0057;
            break;
        default:
            return RelayResult::Ignored;
        }
        Reader reader(motion);
        uint16_t word;
        uint32_t ignored;
        uint8_t flags;
        if (!reader.U16(word) || !reader.U16(word) || reader.VarU32(ignored) != DecodeStatus::Complete)
            return RelayResult::Invalid;
        Buffer message;
        if (decoded.wireId == 0x0017) {
            uint8_t size;
            ByteView blob;
            if (!reader.U8(flags) || reader.VarU32(ignored) != DecodeStatus::Complete || !reader.U8(size) ||
                !reader.Bytes(size, blob))
                return RelayResult::Invalid;
            if (size != 18 || blob.data[0] != 0x0c || blob.data[1] != 2)
                return RelayResult::Ignored;
            const uint8_t prefix[] = {0x4c, 7, 0, 0, 0xdc, 0x42};
            if (!message.U16(serverId) || !message.U16(body) || !message.VarU32(sequence) || !message.U8(0x10) ||
                !message.VarU32(actionTick ? actionTick : 1) || !message.U8(22) ||
                !message.Append(ByteView(prefix, sizeof(prefix))) || !message.Append(ByteView(blob.data + 2, 16)) ||
                !message.U16(0x0047) || !message.U16(body) || !message.VarU32(sequence + 1) || !message.U8(0x10) ||
                !message.U8(0) || !message.U32(0x43930000))
                return RelayResult::Invalid;
        } else {
            if (!message.U16(serverId) || !message.U16(body) || !message.VarU32(sequence))
                return RelayResult::Invalid;
            if (serverId == 0x0034 || serverId == 0x0039 || serverId == 0x0057) {
                if (!reader.U8(flags) || !message.U8(flags | 3))
                    return RelayResult::Invalid;
            }
            if (serverId == 0x0055) {
                // This task stores its authority byte after GUID_OPT, vector and U16.
                const size_t start = reader.Position();
                ByteView bytes;
                if (!reader.Bytes(12, bytes) || reader.VarU32(ignored) != DecodeStatus::Complete ||
                    !reader.Bytes(14, bytes) ||
                    !message.Append(ByteView(motion.data + start, reader.Position() - start)) || !reader.U8(flags) ||
                    !message.U8(flags | 3))
                    return RelayResult::Invalid;
            }
            ByteView rest;
            if (!reader.Bytes(reader.Remaining(), rest) || !message.Append(rest))
                return RelayResult::Invalid;
        }
        out = std::move(message);
        return RelayResult::Encoded;
    }
    RelayResult PeerGlide(ByteView motion, ByteView report, uint16_t body, uint32_t sequence,
                          Buffer& out) const override {
        GameMessage rich, heartbeat;
        CatalogPosition position;
        Facing facing;
        if (!body || body == 57344 || !sequence || sequence == UINT32_MAX ||
            Build204489Wire().DecodeGame(motion, rich) != DecodeStatus::Complete || rich.consumed != motion.size ||
            Build204489Wire().DecodeGame(report, heartbeat) != DecodeStatus::Complete ||
            heartbeat.consumed != report.size || DecodePosition(motion, position) == PositionResult::Invalid ||
            DecodePosition(report, position) == PositionResult::Invalid ||
            DecodeFacing(motion, facing) == FacingResult::Invalid)
            return RelayResult::Invalid;
        if (rich.wireId != 0x000b || heartbeat.wireId != 0x0012 ||
            DecodeFacing(motion, facing) != FacingResult::Present)
            return RelayResult::Ignored;
        Reader source(motion), update(report);
        uint16_t word;
        uint32_t sourceSequence, reportSequence, token, ignored;
        uint8_t flags, size, presence;
        ByteView blob, vector, xyz;
        if (!source.U16(word) || !source.U16(word) || source.VarU32(sourceSequence) != DecodeStatus::Complete ||
            !source.U8(flags) || source.VarU32(token) != DecodeStatus::Complete)
            return RelayResult::Invalid;
        const size_t preservedAt = source.Position();
        if (!source.U8(size) || !source.Bytes(size, blob) || !source.U8(presence) || presence != 1)
            return RelayResult::Invalid;
        const size_t positionAt = source.Position();
        if (!source.Bytes(16, vector) || !update.U16(word) || update.VarU32(reportSequence) != DecodeStatus::Complete ||
            update.VarU32(ignored) != DecodeStatus::Complete || !update.Bytes(12, xyz))
            return RelayResult::Invalid;
        // Retain the old server's wrapping 16-bit movement counter comparison.
        const uint32_t distance = ((reportSequence & 0xffffu) - (sourceSequence & 0xffffu)) & 0xffffu;
        if (!distance || distance >= 0x8000u)
            return RelayResult::Ignored;
        Buffer message;
        if (!message.U16(0x0039) || !message.U16(body) || !message.VarU32(sequence) || !message.U8(flags | 3) ||
            !message.VarU32(token + distance) ||
            !message.Append(ByteView(motion.data + preservedAt, positionAt - preservedAt)) || !message.Append(xyz) ||
            !message.Append(ByteView(vector.data + 12, 4)) ||
            !message.Append(ByteView(motion.data + source.Position(), source.Remaining())))
            return RelayResult::Invalid;
        out = std::move(message);
        return RelayResult::Encoded;
    }
    bool Teleport(ByteView motion, const CatalogPosition& position, uint16_t body, uint32_t sequence,
                  Buffer& out) const {
        Facing facing;
        if (!sequence || sequence == UINT32_MAX || DecodeFacing(motion, facing) != FacingResult::Present ||
            !std::isfinite(position.x * 32) || !std::isfinite(position.y * 32) || !std::isfinite(position.z * 32))
            return false;
        Reader reader(motion);
        uint16_t word;
        uint32_t ignored, token, nested;
        uint8_t flags, size, presence, a, b;
        ByteView blob, vector, skipped;
        if (!reader.U16(word) || !reader.U16(word) || reader.VarU32(ignored) != DecodeStatus::Complete ||
            !reader.U8(flags) || reader.VarU32(token) != DecodeStatus::Complete || !reader.U8(size) ||
            !reader.Bytes(size, blob) || !reader.U8(presence) || !presence || !reader.Bytes(16, vector) ||
            reader.VarU32(nested) != DecodeStatus::Complete)
            return false;
        for (unsigned i = 0; i < 2; ++i)
            if (!reader.U8(presence) || (presence && !reader.Bytes(16, skipped)))
                return false;
        if (!reader.U8(a) || !reader.U8(b))
            return false;
        Buffer message;
        // Retain the previous server's authority update followed by its collision snap.
        if (!message.U16(0x0039) || !message.U16(body) || !message.VarU32(sequence) || !message.U8(flags | 0x10) ||
            !message.VarU32(token + 1) || !message.U8(size) || !message.Append(blob) || !message.U8(1) ||
            !message.F32(position.x) || !message.F32(position.y) || !message.F32(position.z) ||
            !message.Append(ByteView(vector.data + 12, 4)) || !message.VarU32(nested) || !message.U8(0) ||
            !message.U8(0) || !message.U8(a) || !message.U8(b) || !message.F32(0) || !message.F32(0) ||
            !message.F32(0) || !message.F32(facing.x) || !message.F32(facing.y) || !message.F32(facing.z) ||
            !message.U8(0) || !message.U16(0x0040) || !message.U16(body) || !message.VarU32(sequence + 1) ||
            !message.U8(0x10) || !message.F32(position.x * 32) || !message.F32(position.y * 32) ||
            !message.F32(position.z == 0 ? 0 : -position.z * 32) || !message.VarU32(0) || !message.U8(1))
            return false;
        out = std::move(message);
        return true;
    }
    bool DecodeChat(ByteView message, ChatMessage& out) const override {
        GameMessage decoded;
        if (Build204489Wire().DecodeGame(message, decoded) != DecodeStatus::Complete ||
            decoded.action != GameAction::Chat || decoded.consumed != message.size)
            return false;
        Reader reader(message);
        uint16_t unit;
        if (!reader.U16(unit))
            return false;
        std::wstring text;
        for (;;) {
            if (!reader.U16(unit))
                return false;
            if (!unit)
                break;
            if (text.size() == 511)
                return false;
            text += static_cast<wchar_t>(unit);
        }
        ChatMessage chat;
        if (!WideToUtf8(text, chat.text) || chat.text.size() > 511 || !reader.U8(chat.channel) ||
            reader.VarU32(chat.target) != DecodeStatus::Complete || !reader.U8(chat.flags) || reader.Remaining())
            return false;
        out = std::move(chat);
        return true;
    }
    bool Chat(const ChatMessage& chat, const std::string& sender, const Guid& senderGuid, Buffer& out) const override {
        std::wstring text, name;
        if (chat.text.find('\0') != std::string::npos || sender.find('\0') != std::string::npos ||
            !Utf8ToWide(chat.text, text) || !Utf8ToWide(sender, name) || text.size() > 255 || name.size() > 35)
            return false;
        Buffer message;
        // Retained coded-text literal, followed by the sender/channel descriptor.
        if (!message.U16(0x02cc) || !message.U16(0x8101) || !message.U16(0x447b) || !message.U16(0x0107))
            return false;
        for (wchar_t unit : text) {
            if ((unit == '[' || unit == '\\' || unit == ']') && !message.U16('\\'))
                return false;
            if (!message.U16(static_cast<uint16_t>(unit)))
                return false;
        }
        bool anonymous = true;
        for (auto byte : senderGuid.bytes)
            if (byte) {
                anonymous = false;
                break;
            }
        if (!message.U16(1) || !message.U16(0) || !message.U16(0x02cd) || !message.U8(chat.channel) ||
            !message.U8(0x16) ||
            !(anonymous ? AppendCodedText(name, 36, message) : message.Utf16Z(sender.c_str(), 35)) ||
            !message.Append(ByteView(senderGuid.bytes, 16)) || !message.VarU32(chat.target) || !message.VarU32(0) ||
            !message.U8(chat.flags))
            return false;
        out = std::move(message);
        return true;
    }
    bool MapRedirect(uint32_t association, unsigned map, Buffer& out) const override {
        if (!association || !map || map > 65535)
            return false;
        const uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
        uint32_t nonce = 0x50000000u ^ (map << 4) ^ association;
        if (!nonce)
            nonce = 1;
        Buffer message;
        if (!message.U16(0x042a) || !message.Append(ByteView(endpoint, sizeof(endpoint))) || !message.VarU32(0) ||
            !message.VarU32(association) || !message.U16(static_cast<uint16_t>(map)) || !message.VarU32(nonce) ||
            !message.VarU32(0))
            return false;
        out = std::move(message);
        return true;
    }
    bool Signal(Buffer& out) const override {
        Buffer message;
        if (!message.U16(3))
            return false;
        out = std::move(message);
        return true;
    }
    PositionResult DecodePosition(ByteView message, CatalogPosition& out) const override {
        GameMessage decoded;
        if (Build204489Wire().DecodeGame(message, decoded) != DecodeStatus::Complete ||
            decoded.consumed != message.size)
            return PositionResult::Invalid;
        if (decoded.wireId != 0x0012 && decoded.wireId != 0x001a && decoded.wireId != 0x0004 &&
            decoded.wireId != 0x000b)
            return PositionResult::Absent;
        Reader reader(message);
        uint16_t word;
        uint32_t value;
        uint8_t byte;
        ByteView bytes;
        if (!reader.U16(word))
            return PositionResult::Invalid;
        if (decoded.wireId == 0x0012) {
            if (reader.VarU32(value) != DecodeStatus::Complete || reader.VarU32(value) != DecodeStatus::Complete)
                return PositionResult::Invalid;
        } else {
            if (!reader.U16(word) || reader.VarU32(value) != DecodeStatus::Complete || !reader.U8(byte))
                return PositionResult::Invalid;
            if (decoded.wireId == 0x000b) {
                if (reader.VarU32(value) != DecodeStatus::Complete || !reader.U8(byte) || !reader.Bytes(byte, bytes))
                    return PositionResult::Invalid;
            }
            if (decoded.wireId != 0x001a) {
                if (!reader.U8(byte))
                    return PositionResult::Invalid;
                if (!byte)
                    return PositionResult::Absent;
            }
        }
        if (!reader.Bytes(12, bytes))
            return PositionResult::Invalid;
        float position[3];
        std::memcpy(position, bytes.data, sizeof(position));
        for (float coordinate : position)
            if (!std::isfinite(coordinate * 32))
                return PositionResult::Invalid;
        out = CatalogPosition{position[0], position[1], position[2]};
        return PositionResult::Present;
    }
    FacingResult DecodeFacing(ByteView message, Facing& out) const override {
        GameMessage decoded;
        if (Build204489Wire().DecodeGame(message, decoded) != DecodeStatus::Complete ||
            decoded.consumed != message.size)
            return FacingResult::Invalid;
        if (decoded.wireId != 0x000b)
            return FacingResult::Absent;
        Reader reader(message);
        uint16_t word;
        uint32_t value;
        uint8_t byte;
        ByteView bytes;
        if (!reader.U16(word) || !reader.U16(word) || reader.VarU32(value) != DecodeStatus::Complete ||
            !reader.U8(byte) || reader.VarU32(value) != DecodeStatus::Complete || !reader.U8(byte) ||
            !reader.Bytes(byte, bytes) || !reader.U8(byte))
            return FacingResult::Invalid;
        // The retained motion decoder only trusts facing alongside a locomotion record.
        if (!byte)
            return FacingResult::Absent;
        if (!reader.Bytes(16, bytes) || reader.VarU32(value) != DecodeStatus::Complete)
            return FacingResult::Invalid;
        for (unsigned i = 0; i < 2; ++i) {
            if (!reader.U8(byte) || (byte && !reader.Bytes(16, bytes)))
                return FacingResult::Invalid;
        }
        if (!reader.U8(byte) || !reader.U8(byte) || !reader.Bytes(12, bytes) || !reader.Bytes(12, bytes))
            return FacingResult::Invalid;
        float facing[3];
        std::memcpy(facing, bytes.data, sizeof(facing));
        for (float coordinate : facing)
            if (!std::isfinite(coordinate))
                return FacingResult::Invalid;
        out = Facing{facing[0], facing[1], facing[2]};
        return FacingResult::Present;
    }
};
} // namespace
const GameMessages& Build204489Game() {
    static Build204489GameMessages messages;
    return messages;
}
} // namespace glide
