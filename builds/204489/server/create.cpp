#include "glide/profile/create_flow.h"
#include "glide/profile/auth_codec.h"
#include "glide/files.h"
#include "glide/profile/wire_map.h"

namespace glide {
const AuthCodec& Build204489Auth();
const WireMap& Build204489Wire();
namespace {
std::string Hex(ByteView bytes) {
    const char* digits = "0123456789abcdef";
    std::string text;
    for (size_t i = 0; i < bytes.size; ++i) {
        text += digits[bytes.data[i] >> 4];
        text += digits[bytes.data[i] & 15];
    }
    return text;
}
DecodeStatus String(Reader& reader, size_t maximum, std::string& text) {
    std::wstring wide;
    for (size_t i = 0; i <= maximum; ++i) {
        uint16_t unit;
        if (!reader.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit)
            return WideToUtf8(wide, text) ? DecodeStatus::Complete : DecodeStatus::Malformed;
        if (i == maximum)
            return DecodeStatus::Malformed;
        wide += static_cast<wchar_t>(unit);
    }
    return DecodeStatus::Malformed;
}
class CreateFlow204489 : public CreateFlow {
  public:
    const SpawnPoint& InitialLocation() const override {
        // Catalog waypoint 1036, one unit above its height.
        static const SpawnPoint point{50, -435.774994f, 464.728119f, 38.9903107f};
        return point;
    }
    DecodeStatus Decode(ByteView input, CreateMessage& out) const override {
        if (!input.Valid())
            return DecodeStatus::Malformed;
        Reader reader(input);
        uint16_t id;
        if (!reader.U16(id))
            return DecodeStatus::Incomplete;
        CreateMessage message;
        if (id == 1)
            message.action = CreateAction::Close;
        else if (id == 0x01af)
            message.action = CreateAction::Complete;
        else if (id == 2) {
            uint32_t elapsed;
            const auto status = reader.VarU32(elapsed);
            if (status != DecodeStatus::Complete)
                return status;
        } else if (id == 0x0199 || id == 0x019a || id == 0x019b || id == 0x019c || id == 0x019d || id == 0x01a4 ||
                   id == 0x01a5 || id == 0x01a7) {
            // Native fieldless loading controls retained from the existing Create session.
        } else if (id == 0x01a9) {
            std::string first, second;
            auto status = String(reader, 64, first);
            if (status != DecodeStatus::Complete)
                return status;
            status = String(reader, 64, second);
            if (status != DecodeStatus::Complete)
                return status;
            ByteView flags;
            if (!reader.Bytes(2, flags))
                return DecodeStatus::Incomplete;
            message.action = CreateAction::Start;
        } else if (id == 0x01a0 || id == 0x01a1) {
            const bool preview = id == 0x01a1;
            uint8_t discriminator;
            if (preview && !reader.U8(discriminator))
                return DecodeStatus::Incomplete;
            auto status = String(reader, 20, message.character.name);
            if (status != DecodeStatus::Complete)
                return status;
            ByteView appearance, biography;
            uint8_t profession, requestedLevel, count;
            uint32_t flags;
            if (!reader.Bytes(48, appearance) || !reader.U8(profession) || !reader.U8(requestedLevel))
                return DecodeStatus::Incomplete;
            status = reader.VarU32(flags);
            if (status != DecodeStatus::Complete)
                return status;
            if (!reader.U8(count))
                return DecodeStatus::Incomplete;
            if (count > 32)
                return DecodeStatus::Malformed;
            if (!reader.Bytes(count, biography))
                return DecodeStatus::Incomplete;
            Buffer colors;
            for (unsigned i = 0; i < 24; ++i) {
                uint32_t value;
                status = reader.VarU32(value);
                if (status != DecodeStatus::Complete)
                    return status;
                if (!colors.U32(value))
                    return DecodeStatus::Malformed;
            }
            auto& character = message.character;
            character.level = 80;
            character.glider = 154;
            auto& fields = character.profileFields;
            fields["profession"] = std::to_string(profession);
            fields["race"] = std::to_string(appearance.data[27]);
            fields["body_scale"] = std::to_string(appearance.data[28]);
            fields["sex"] = std::to_string(appearance.data[29]);
            fields["appearance"] = Hex(ByteView(appearance.data, 45));
            fields["appearance_tail"] = Hex(ByteView(appearance.data + 45, 3));
            fields["creation_flags"] = std::to_string(flags);
            fields["biography"] = Hex(biography);
            fields["creation_colors"] = Hex(colors.View());
            fields["created_from_client"] = "1";
            fields["effective_level"] = "80";
            fields["requested_level"] = std::to_string(requestedLevel);
            message.action = preview ? CreateAction::Preview : CreateAction::Submit;
        } else {
            // The native creation connection also carries the regular greeting and controls.
            // Reuse their schemas; never discard an unknown remainder as the old server did.
            GameMessage shared;
            const auto status = Build204489Wire().DecodeGame(input, shared);
            if (status != DecodeStatus::Complete)
                return status;
            message.consumed = shared.consumed;
            out = std::move(message);
            return DecodeStatus::Complete;
        }
        message.consumed = reader.Position();
        out = std::move(message);
        return DecodeStatus::Complete;
    }
    bool ValidateSubmit(const Character& character) const override {
        std::wstring name;
        if (!Utf8ToWide(character.name, name) || name.empty() || name.size() >= 20)
            return false;
        for (wchar_t unit : name)
            if (unit < 32 || unit == 127)
                return false;
        const auto at = character.profileFields.find("requested_level");
        unsigned level;
        if (at == character.profileFields.end() || !ParseUnsigned(at->second, 80, level) || !level)
            return false;
        Buffer summary;
        return Build204489Auth().Summary(character, summary);
    }
    bool Ready(Buffer& out) const override {
        const uint8_t bytes[] = {0x0f, 4, 0xff, 0, 0, 0, 0x1b, 4, 3, 0};
        Buffer result;
        if (!result.Append(ByteView(bytes, sizeof(bytes))))
            return false;
        out = std::move(result);
        return true;
    }
    bool Signal(Buffer& out) const override {
        Buffer result;
        if (!result.U16(3))
            return false;
        out = std::move(result);
        return true;
    }
    bool Created(const Guid& account, const Character& character, Buffer& out) const override {
        Buffer summary, result;
        if (!ValidateSubmit(character) || !Build204489Auth().Summary(character, summary) || !result.U16(0x0419) ||
            !result.Append(ByteView(character.guid.bytes, 16)) || !result.Append(ByteView(account.bytes, 16)) ||
            !result.Utf16Z(character.name.c_str(), 20) || !result.U16(0) || !result.U64(0) || !result.U64(0) ||
            !result.U16(static_cast<uint16_t>(summary.Size())) || !result.Append(summary.View()))
            return false;
        out = std::move(result);
        return true;
    }
};
} // namespace
const CreateFlow& Build204489Create() {
    static CreateFlow204489 flow;
    return flow;
}
} // namespace glide
