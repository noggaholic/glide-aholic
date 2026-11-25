#include "create.h"
#include "game_request.h"
#include "character_catalog.h"
#include "glide/files.h"
#include "glide/profile/character_starting_level.h"
#include <cmath>

namespace glide {
namespace june14505 {
namespace {
std::string Hex(ByteView bytes) {
    const char* digits = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < bytes.size; ++i) {
        result += digits[bytes.data[i] >> 4];
        result += digits[bytes.data[i] & 15];
    }
    return result;
}
DecodeStatus Text(Reader& reader, size_t capacity, std::string& out) {
    std::wstring wide;
    for (size_t i = 0; i < capacity; ++i) {
        uint16_t value;
        if (!reader.U16(value))
            return DecodeStatus::Incomplete;
        if (!value)
            return WideToUtf8(wide, out) ? DecodeStatus::Complete : DecodeStatus::Malformed;
        wide += static_cast<wchar_t>(value);
    }
    return DecodeStatus::Malformed;
}
struct CreateProtocol {
    uint16_t clientState, clientStart, clientSubmit, clientReady;
    uint16_t serverState, serverReady, serverScene, serverCreated;
};
const CreateProtocol aprilProtocol = {0x0074, 0x0075, 0x006e, 0x006c, 0x0176, 0x017b, 0x0184, 0x0179};
const CreateProtocol juneProtocol = {0x009a, 0x009b, 0x0093, 0x0091, 0x01d2, 0x01d6, 0x01de, 0x01d4};
class BetaCreate : public CreateFlow {
  public:
    bool WorldRequestThroughAuth() const override {
        return true;
    }
    CharacterCatalog catalog;
    CreateProtocol protocol = juneProtocol;
    SpawnPoint initial{};
    const SpawnPoint& InitialLocation() const override {
        return initial;
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
        else if (catalog.Build() != 14075 && id == 0x0059) {
            // June can send DebugInfo before Mission start on a reused client.
            // Use the same bounded reader as the world connection. Telemetry
            // is a control only; it cannot start creation or save a character.
            GameRequest report;
            const auto status = DecodeGameRequest(input, report);
            if (status != DecodeStatus::Complete)
                return status;
            if (report.kind != GameRequestKind::Debug)
                return DecodeStatus::Malformed;
            message.consumed = report.consumed;
            out = std::move(message);
            return DecodeStatus::Complete;
        } else if (id == 2) {
            uint32_t elapsed;
            const auto status = reader.VarU32(elapsed);
            if (status != DecodeStatus::Complete)
                return status;
        } else if (protocol.clientReady && id == protocol.clientReady) {
            // Mission readiness acknowledgement, not a world redirect request.
        } else if (id == protocol.clientState) {
            uint16_t state;
            if (!reader.U16(state))
                return DecodeStatus::Incomplete;
        } else if (id == protocol.clientStart) {
            std::string text;
            const auto status = Text(reader, 64, text);
            if (status != DecodeStatus::Complete)
                return status;
            uint16_t flags;
            if (!reader.U16(flags))
                return DecodeStatus::Incomplete;
            message.action = CreateAction::Start;
        } else if (id == protocol.clientSubmit) {
            auto status = Text(reader, 20, message.character.name);
            if (status != DecodeStatus::Complete)
                return status;
            ByteView appearance, biography;
            uint8_t profession, level, count;
            uint32_t flags;
            if (!reader.Bytes(48, appearance) || !reader.U8(profession) || !reader.U8(level))
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
            std::string colors;
            for (unsigned i = 0; i < 24; ++i) {
                uint32_t color;
                status = reader.VarU32(color);
                if (status != DecodeStatus::Complete)
                    return status;
                if (!catalog.HasColor(color))
                    return DecodeStatus::Malformed;
                if (i)
                    colors += ',';
                colors += std::to_string(color);
            }
            const auto* recipe = catalog.Find(appearance.data[27], appearance.data[29], profession, level);
            if (!recipe)
                return DecodeStatus::Malformed;
            auto& character = message.character;
            character.level = kMaximumCharacterLevel;
            auto& fields = character.profileFields;
            fields["profession"] = std::to_string(profession);
            fields["race"] = std::to_string(appearance.data[27]);
            fields["body_scale"] = std::to_string(appearance.data[28]);
            fields["sex"] = std::to_string(appearance.data[29]);
            fields["appearance"] = Hex(ByteView(appearance.data, 45));
            fields["appearance_tail"] = Hex(ByteView(appearance.data + 45, 3));
            fields["creation_flags"] = std::to_string(flags);
            fields["biography"] = Hex(biography);
            fields["armor_colors"] = std::move(colors);
            fields["created_from_client"] = "1";
            fields["effective_level"] = std::to_string(kMaximumCharacterLevel);
            fields["requested_level"] = std::to_string(level);
            std::string armor;
            for (unsigned item : recipe->armor) {
                if (!armor.empty())
                    armor += ',';
                armor += std::to_string(item);
            }
            fields["armor_items"] = std::move(armor);
            if (!ValidateSubmit(character))
                return DecodeStatus::Malformed;
            message.action = CreateAction::Submit;
        } else
            return DecodeStatus::Unknown;
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
        RosterCharacter roster;
        std::string error;
        return catalog.Resolve(character, roster, error);
    }
    bool Ready(Buffer& out) const override {
        Buffer result;
        if (!result.U16(protocol.serverState) || !result.U32(0xff) || !result.U16(protocol.serverReady) ||
            !result.U16(protocol.serverScene) || !result.U16(3))
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
        RosterCharacter roster;
        std::string error;
        Buffer summary, result;
        if (!ValidateSubmit(character) || !catalog.Resolve(character, roster, error) ||
            !CharacterSummary(roster, summary) || !result.U16(protocol.serverCreated) ||
            !result.Append(ByteView(character.guid.bytes, 16)) || !result.Append(ByteView(account.bytes, 16)) ||
            !result.Utf16Z(character.name.c_str(), 19) || !result.U16(0) ||
            !result.U16(static_cast<uint16_t>(summary.Size())) || !result.Append(summary.View()))
            return false;
        out = std::move(result);
        return true;
    }
};
} // namespace
bool CreateCharacterFlowFromCatalog(CharacterCatalog catalog, const SpawnPoint& initial,
                                    std::unique_ptr<CreateFlow>& out, std::string& error) {
    if (!initial.map || initial.map > 65535 || !std::isfinite(initial.x * 32) || !std::isfinite(initial.y * 32) ||
        !std::isfinite(initial.z * 32)) {
        error = "Invalid beta starting location";
        return false;
    }
    if ((catalog.Build() != 14075 && catalog.Build() != 14422 && catalog.Build() != 14505) || !catalog.RecipeCount() ||
        !catalog.ColorCount()) {
        error = "Missing or unsupported beta character catalog";
        return false;
    }
    std::unique_ptr<BetaCreate> flow(new BetaCreate);
    flow->protocol = catalog.Build() == 14075 ? aprilProtocol : juneProtocol;
    flow->catalog = std::move(catalog);
    flow->initial = initial;
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    if (!initial.map || initial.map > 65535 || !std::isfinite(initial.x * 32) || !std::isfinite(initial.y * 32) ||
        !std::isfinite(initial.z * 32)) {
        error = "Invalid June starting location";
        return false;
    }
    CharacterCatalog catalog;
    if (!catalog.Load(dataRoot + L"/14505/character-recipes.txt", error) ||
        !catalog.LoadWeapons(dataRoot + L"/14505/weapon-loadouts.txt", error))
        return false;
    return CreateCharacterFlowFromCatalog(std::move(catalog), initial, out, error);
}
} // namespace june14505
} // namespace glide
