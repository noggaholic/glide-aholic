#include "create.h"
#include "character_create.h"
#include "auth_bytes.h"
#include <cmath>
namespace glide {
namespace july14812 {
namespace {
DecodeStatus Control(Reader& reader, uint16_t id, CreateAction& action) {
    uint32_t value = 0;
    uint16_t word = 0;
    uint8_t byte = 0;
    switch (id) {
    case 1:
        action = CreateAction::Close;
        return DecodeStatus::Complete;
    case 145:
        action = CreateAction::Acknowledge;
        return DecodeStatus::Complete;
    case 2:
        action = CreateAction::StartedControl;
        return reader.VarU32(value);
    case 155: {
        std::string discarded;
        const auto status = detail::Text(reader, 64, discarded);
        if (status != DecodeStatus::Complete)
            return status;
        action = CreateAction::Start;
        return reader.U8(byte) && reader.U8(byte) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 154: {
        uint8_t first = 0, second = 0;
        if (!reader.U8(first) || !reader.U8(second))
            return DecodeStatus::Incomplete;
        if (first > 4 || second > 4)
            return DecodeStatus::Malformed;
        action = CreateAction::Preview;
        return DecodeStatus::Complete;
    }
    case 89: {
        auto status = reader.VarU32(value);
        if (status != DecodeStatus::Complete)
            return status;
        uint8_t count;
        if (!reader.U8(count))
            return DecodeStatus::Incomplete;
        if (count > 32)
            return DecodeStatus::Malformed;
        for (unsigned i = 0; i < count; ++i) {
            status = reader.VarU32(value);
            if (status != DecodeStatus::Complete)
                return status;
        }
        return DecodeStatus::Complete;
    }
    case 120: {
        // Native telemetry: validate its shape, discard command line/hardware text.
        static const uint16_t fields[][2] = {{2, 0}, {13, 256}, {3, 0}, {3, 0}, {4, 0}, {4, 0},
                                             {4, 0}, {4, 0},    {2, 0}, {3, 0}, {4, 0}, {13, 13},
                                             {3, 0}, {3, 0},    {3, 0}, {3, 0}, {3, 0}, {4, 0}};
        for (const auto& field : fields) {
            if (field[0] == 2) {
                if (!reader.U8(byte))
                    return DecodeStatus::Incomplete;
            } else if (field[0] == 3) {
                if (!reader.U16(word))
                    return DecodeStatus::Incomplete;
            } else if (field[0] == 4) {
                const auto status = reader.VarU32(value);
                if (status != DecodeStatus::Complete)
                    return status;
            } else {
                std::string discarded;
                const auto status = detail::Text(reader, field[1], discarded);
                if (status != DecodeStatus::Complete)
                    return status;
            }
        }
        return DecodeStatus::Complete;
    }
    default:
        return DecodeStatus::Unknown;
    }
}
class JulyCreate final : public CreateFlow {
  public:
    CharacterCatalog catalog;
    SpawnPoint initial{};
    bool WorldRequestThroughAuth() const override {
        return true;
    }
    bool RequiresReadyAcknowledgment() const override {
        return true;
    }
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
        CreateMessage result;
        if (id == 147) {
            const auto status = DecodeSubmission(input, catalog, result.character, result.consumed);
            if (status != DecodeStatus::Complete)
                return status;
            if (!ValidateSubmit(result.character))
                return DecodeStatus::Malformed;
            result.action = CreateAction::Submit;
        } else {
            const auto status = DecodeSceneControl(input, result);
            if (status != DecodeStatus::Complete)
                return status;
        }
        out = std::move(result);
        return DecodeStatus::Complete;
    }
    bool ValidateSubmit(const Character& saved) const override {
        std::wstring name;
        std::vector<uint8_t> bytes;
        RosterEntry entry;
        std::string error;
        if (saved.name.find('\0') != std::string::npos || !Utf8ToWide(saved.name, name) || name.empty() ||
            name.size() > 19)
            return false;
        for (auto unit : name)
            if (unit < 32 || unit == 127)
                return false;
        const auto flags = saved.profileFields.find("creation_flags"),
                   tail = saved.profileFields.find("appearance_tail"), bio = saved.profileFields.find("biography");
        if (flags == saved.profileFields.end() || flags->second != "0" || tail == saved.profileFields.end() ||
            bio == saved.profileFields.end() || !DecodeHex(tail->second, 3, bytes) || bytes.size() != 3 ||
            !DecodeHex(bio->second, 32, bytes))
            return false;
        return catalog.Resolve(saved, entry, error);
    }
    bool Ready(Buffer& out) const override {
        const uint8_t scene[] = {0xd2, 1, 0, 0, 0, 0, 0xd6, 1, 0xde, 1, 3, 0};
        Buffer result;
        if (!result.Append(ByteView(scene, sizeof(scene))))
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
    bool Created(const Guid& account, const Character& saved, Buffer& out) const override {
        return ValidateSubmit(saved) && CreatedReply(account, saved, catalog, out);
    }
};
} // namespace
DecodeStatus DecodeSceneControl(ByteView input, CreateMessage& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    CreateMessage result;
    const auto status = Control(reader, id, result.action);
    if (status != DecodeStatus::Complete)
        return status;
    result.consumed = reader.Position();
    out = std::move(result);
    return DecodeStatus::Complete;
}
const SpawnPoint& InitialSpawn() {
    static const SpawnPoint point{15, -15525.0f / 32, 21974.0f / 32, 2506.0f / 32};
    return point;
}
bool CreateCharacterFlow(CharacterCatalog catalog, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    error = "Invalid July construction matrix or initial location";
    if (catalog.RecipeCount() != 48 || catalog.Palette().size() != 391 || initial.map != 15 ||
        !std::isfinite(initial.x * 32) || !std::isfinite(initial.y * 32) || !std::isfinite(initial.z * 32))
        return false;
    std::unique_ptr<JulyCreate> flow(new JulyCreate);
    flow->catalog = std::move(catalog);
    flow->initial = initial;
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateCharacterFlow(const std::wstring& root, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.Load(root + L"/14812/construction.bin", error))
        return false;
    return CreateCharacterFlow(std::move(catalog), initial, out, error);
}
} // namespace july14812
} // namespace glide
