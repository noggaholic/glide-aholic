#include "glide/profile/auth_codec.h"
#include <cstring>
#include "glide/files.h"

namespace glide {
namespace {
DecodeStatus Utf16(Reader& reader, size_t limit) {
    for (size_t i = 0; i <= limit; ++i) {
        uint16_t value;
        if (!reader.U16(value))
            return DecodeStatus::Incomplete;
        if (!value)
            return DecodeStatus::Complete;
        if (i == limit)
            return DecodeStatus::Malformed;
    }
    return DecodeStatus::Malformed;
}
class Build204489AuthCodec : public AuthCodec {
    const CharacterCodec* characters_;

  public:
    explicit Build204489AuthCodec(const CharacterCodec* characters = nullptr) : characters_(characters) {}
    std::unique_ptr<AuthCodec> WithCharacters(const CharacterCodec& characters) const override {
        return std::unique_ptr<AuthCodec>(new Build204489AuthCodec(&characters));
    }
    bool Assignment(const AuthMessage& message, uint32_t association, uint32_t map, uint32_t nonce,
                    Buffer& out) const override {
        const uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
        Buffer payload;
        if (!payload.U16(20) || !payload.VarU32(message.request) || !payload.VarU32(message.route) ||
            !payload.VarU32(association) || !payload.VarU32(map) ||
            !payload.Append(ByteView(endpoint, sizeof(endpoint))) || !payload.VarU32(nonce))
            return false;
        out = std::move(payload);
        return true;
    }
    static bool AccountObject(const AccountState& account, uint32_t request, uint32_t seconds, Buffer& payload) {
        const Guid* selected = nullptr;
        for (const auto& character : account.characters)
            if (character.slot == account.selected)
                selected = &character.guid;
        if (!account.characters.empty() && !selected)
            return false;
        const uint8_t zero[32] = {};
        if (!payload.U16(8) || !payload.VarU32(request) || !payload.Append(ByteView(account.userGuid.bytes, 16)) ||
            !payload.Append(ByteView(selected ? selected->bytes : zero, 16)))
            return false;
        const uint32_t properties[] = {account.accessLevel, 13, seconds, account.homeWorld, account.homeWorld,
                                       account.worldClass,  0};
        for (uint32_t property : properties)
            if (!payload.VarU32(property))
                return false;
        return payload.U8(0) && payload.Append(ByteView(zero, 32)) && payload.U8(1) && payload.U16(45) &&
               payload.U16(1) && payload.VarU32(0) && payload.VarU32(0) && payload.VarU32(0) &&
               payload.Append(ByteView(zero, 32));
    }
    static bool Ack(uint32_t request, uint32_t a, uint32_t b, uint32_t c, Buffer& payload) {
        return payload.U16(4) && payload.VarU32(request) && payload.VarU32(0) && payload.VarU32(a) &&
               payload.VarU32(b) && payload.VarU32(c);
    }
    bool Reply(const AuthMessage& message, Buffer& out) const override {
        uint32_t code;
        if (message.id == 12)
            code = 3819;
        else if (message.id == 14)
            code = 4554;
        else if (message.id == 39)
            code = 5121;
        else
            return false;
        Buffer payload;
        if (!Ack(message.request, 4, 5, code, payload))
            return false;
        out = std::move(payload);
        return true;
    }
    bool Deletion(uint32_t request, Buffer& out) const override {
        Buffer payload;
        if (!request || !Ack(request, 0, 0, 0, payload))
            return false;
        out = std::move(payload);
        return true;
    }
    bool Selection(const AccountState* account, uint32_t request, uint32_t seconds, Buffer& out) const override {
        Buffer payload;
        if (account && !AccountObject(*account, request, seconds, payload))
            return false;
        if (!Ack(request, account ? 1002 : 0, account ? 3 : 0, account ? 3516 : 0, payload))
            return false;
        out = std::move(payload);
        return true;
    }
    bool SessionStatus(uint64_t connection, Buffer& out) const override {
        Buffer result;
        if (!result.U16(2) || !result.VarU32(0xc0000000u | (static_cast<uint32_t>(connection) & 0x0fffffffu)))
            return false;
        out = std::move(result);
        return true;
    }
    bool Roster(const AccountState& account, uint32_t request, Buffer& payload) const {
        for (const auto& character : account.characters) {
            Buffer summary;
            if (!Summary(character, summary) || !payload.U16(17) || !payload.VarU32(request) ||
                !payload.Append(ByteView(character.guid.bytes, 16)) || !payload.VarU32(0) || !payload.U64(0) ||
                !payload.U64(0) || !payload.Utf16Z(character.name.c_str(), 20) ||
                !payload.U16(static_cast<uint16_t>(summary.Size())) || !payload.Append(summary.View()))
                return false;
        }
        return true;
    }
    bool CharacterList(const AccountState& account, uint32_t request, std::vector<Buffer>& frames) const override {
        std::vector<Buffer> result;
        Buffer start, roster;
        if (!start.U16(23) || !start.U16(24) || !Roster(account, request, roster) || !Ack(request, 0, 0, 0, roster))
            return false;
        result.push_back(std::move(start));
        result.push_back(std::move(roster));
        frames = std::move(result);
        return true;
    }
    bool Summary(const Character& character, Buffer& out) const override {
        if (characters_)
            return characters_->Summary(character, out);
        auto number = [&](const char* key, unsigned maximum, unsigned& value) {
            const auto at = character.profileFields.find(key);
            return at != character.profileFields.end() && ParseUnsigned(at->second, maximum, value);
        };
        unsigned profession, race, sex, scale;
        if (!character.level || character.level > 80 || !number("profession", 9, profession) || !profession ||
            !number("race", 4, race) || !number("sex", 1, sex) || !number("body_scale", 15, scale))
            return false;
        const auto at = character.profileFields.find("appearance");
        std::vector<uint8_t> appearance;
        if (at == character.profileFields.end() || !DecodeHex(at->second, 45, appearance) || appearance.size() != 45)
            return false;
        uint8_t bytes[155] = {11, 0x73, 1, 3};
        bytes[4] = static_cast<uint8_t>(character.level);
        bytes[5] = static_cast<uint8_t>(profession);
        std::memcpy(bytes + 6, appearance.data(), 45);
        bytes[33] = static_cast<uint8_t>(race);
        bytes[34] = static_cast<uint8_t>(scale);
        bytes[35] = static_cast<uint8_t>(sex);
        uint32_t crc = UINT32_MAX;
        for (uint8_t byte : bytes) {
            crc ^= byte;
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
        }
        Buffer result;
        if (!result.Append(ByteView(bytes, sizeof(bytes))) || !result.U32(~crc))
            return false;
        out = std::move(result);
        return true;
    }
    bool Login(const AccountState& account, uint32_t handshake, uint32_t request, uint32_t elapsed,
               uint32_t secondsSince2000, std::vector<Buffer>& frames) const override {
        std::vector<Buffer> result;
        Buffer payload;
        auto finish = [&] {
            result.push_back(std::move(payload));
            payload = Buffer();
        };
        const uint8_t tuple[] = {3, 42, 12};
        if (!payload.U16(1) || !payload.VarU32(handshake) || !payload.U16(15) || !payload.U16(10) || !payload.U8(0) ||
            !payload.U8(24) || !payload.U8(160) || !payload.Append(ByteView(tuple, sizeof(tuple))))
            return false;
        finish();
        if (!payload.U16(30) || !payload.U8(3) || !payload.U8(0))
            return false;
        finish();
        if (!Roster(account, request, payload) || !payload.U16(10) || !payload.VarU32(request) || !payload.U16(0))
            return false;
        finish();
        if (!payload.U16(27) || !payload.VarU32(request) || !payload.U8(1) || !payload.VarU32(45) || !payload.VarU32(1))
            return false;
        finish();
        if (!payload.U16(29) || !payload.VarU32(account.instanceType))
            return false;
        finish();
        if (!AccountObject(account, request, secondsSince2000, payload) || !payload.U16(16) ||
            !payload.VarU32(request) || !payload.Append(ByteView(account.userGuid.bytes, 16)) || !payload.VarU32(0) ||
            !payload.VarU32(elapsed) || !payload.VarU32(0) || !payload.VarU32(0) || !payload.VarU32(0) ||
            !payload.U16(23) || !payload.U16(24))
            return false;
        finish();
        if (!Ack(request, 0, 0, 0, payload))
            return false;
        finish();
        frames = std::move(result);
        return true;
    }
    DecodeStatus Decode(ByteView input, bool authenticated, AuthMessage& out) const override {
        if (!input.Valid())
            return DecodeStatus::Malformed;
        Reader reader(input);
        AuthMessage message;
        if (!reader.U16(message.id))
            return DecodeStatus::Incomplete;
        DecodeStatus status = DecodeStatus::Complete;
        size_t fixed = 0;
        switch (message.id) {
        case 0x0002:
            fixed = 53;
            break;
        case 0x0004:
            fixed = 115;
            break;
        case 0x0400:
            fixed = 119;
            break;
        case 0x0027:
            fixed = 40;
            break;
        case 0x000b:
        case 0x0022:
            fixed = 3;
            break;
        case 0x000e:
            fixed = 6;
            break;
        case 0x0023:
            fixed = 2;
            break;
        case 0x0019:
            status = reader.VarU32(message.request);
            break;
        case 0x0001:
            status = reader.VarU32(message.handshake);
            if (status != DecodeStatus::Complete)
                return status;
            if (!authenticated) {
                uint16_t loginId;
                if (!reader.U16(loginId))
                    return DecodeStatus::Incomplete;
                if (loginId != 0x000a)
                    return DecodeStatus::Malformed;
                status = reader.VarU32(message.request);
                if (status != DecodeStatus::Complete)
                    return status;
                if (!reader.Bytes(32, message.identity))
                    return DecodeStatus::Incomplete;
                status = Utf16(reader, 64);
                if (status != DecodeStatus::Complete)
                    return status;
                status = Utf16(reader, 28);
            }
            break;
        case 0x0014:
        case 0x0016: {
            status = reader.VarU32(message.request);
            if (status != DecodeStatus::Complete)
                return status;
            const size_t begin = reader.Position();
            status = Utf16(reader, message.id == 20 ? 19 : 20);
            if (status != DecodeStatus::Complete)
                return status;
            std::wstring name;
            for (size_t at = begin; at + 2 < reader.Position(); at += 2)
                name += static_cast<wchar_t>(input.data[at] | (static_cast<unsigned>(input.data[at + 1]) << 8));
            if (!WideToUtf8(name, message.characterName))
                return DecodeStatus::Malformed;
            break;
        }
        case 0x001d: {
            status = reader.VarU32(message.request);
            if (status != DecodeStatus::Complete)
                return status;
            uint32_t ignored;
            uint32_t* fields[] = {&message.route, &ignored, &message.map, &ignored, &message.context};
            for (auto* field : fields) {
                status = reader.VarU32(*field);
                if (status != DecodeStatus::Complete)
                    return status;
            }
            ByteView guid;
            if (!reader.Bytes(16, guid))
                return DecodeStatus::Incomplete;
            break;
        }
        case 0x000c: {
            status = reader.VarU32(message.request);
            if (status != DecodeStatus::Complete)
                return status;
            uint16_t length;
            ByteView blob;
            if (!reader.U16(length) || !reader.Bytes(length, blob))
                return DecodeStatus::Incomplete;
            break;
        }
        default:
            return DecodeStatus::Unknown;
        }
        if (status != DecodeStatus::Complete)
            return status;
        if (fixed && input.size < fixed)
            return DecodeStatus::Incomplete;
        if (message.id == 14 || message.id == 39) {
            // The native fixed records begin with a compact request ID.
            Reader fields(ByteView(input.data + 2, fixed - 2));
            if (fields.VarU32(message.request) != DecodeStatus::Complete)
                return DecodeStatus::Malformed;
        }
        message.consumed = fixed ? fixed : reader.Position();
        if (message.id == 1)
            message.action = authenticated ? AuthAction::Control : AuthAction::Login;
        else if (message.id == 2)
            message.action = AuthAction::SessionStatus;
        else if (message.id == 20)
            message.action = AuthAction::DeleteCharacter;
        else if (message.id == 22)
            message.action = AuthAction::SelectCharacter;
        else if (message.id == 25)
            message.action = AuthAction::CharacterList;
        else if (message.id == 29)
            message.action = AuthAction::Assignment;
        else if (message.id == 12 || message.id == 14 || message.id == 39)
            message.action = AuthAction::Reply;
        else if (message.id == 4 || message.id == 0x400 || message.id == 11 || message.id == 34 || message.id == 35)
            message.action = AuthAction::Control;
        out = message;
        return DecodeStatus::Complete;
    }
};
} // namespace
const AuthCodec& Build204489Auth() {
    static Build204489AuthCodec codec;
    return codec;
}
} // namespace glide
