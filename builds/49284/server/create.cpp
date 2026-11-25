#include "create.h"
#include "character_create.h"
#include "control_request.h"
#include "glide/files.h"
#include <cmath>
namespace glide {
namespace hot49284 {
namespace {
class HotCreate final : public CreateFlow {
  public:
    CharacterCatalog catalog;
    SpawnPoint initial{};
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
        if (id == 251) {
            const auto status = DecodeSubmission(input, catalog, result.character, result.consumed);
            if (status != DecodeStatus::Complete)
                return status;
            if (!ValidateSubmit(result.character))
                return DecodeStatus::Malformed;
            result.action = CreateAction::Submit;
            out = std::move(result);
            return DecodeStatus::Complete;
        }
        if (id == 249)
            result.action = CreateAction::Acknowledge;
        else if (id == 262)
            result.action = CreateAction::Complete;
        else {
            const auto status = DecodeControl(input, id, result.consumed);
            if (status != DecodeStatus::Complete)
                return status;
            switch (id) {
            case 1:
                result.action = CreateAction::Close;
                break;
            case 2:
                result.action = CreateAction::StartedControl;
                break;
            case 256:
                result.action = CreateAction::Preview;
                break;
            case 257:
                result.action = CreateAction::Start;
                break;
            default:
                break;
            }
            out = std::move(result);
            return DecodeStatus::Complete;
        }
        result.consumed = reader.Position();
        out = std::move(result);
        return DecodeStatus::Complete;
    }

    bool ValidateSubmit(const Character& character) const override {
        std::wstring name;
        RosterEntry entry;
        std::string error;
        std::vector<uint8_t> bytes;
        if (!Utf8ToWide(character.name, name) || name.empty() || name.size() > 19)
            return false;
        for (auto unit : name)
            if (unit < 32 || unit == 127)
                return false;
        const auto flags = character.profileFields.find("creation_flags"),
                   tail = character.profileFields.find("appearance_tail"),
                   bio = character.profileFields.find("biography");
        if (flags == character.profileFields.end() || flags->second != "0" || tail == character.profileFields.end() ||
            bio == character.profileFields.end() || !DecodeHex(tail->second, 3, bytes) || bytes.size() != 3 ||
            !DecodeHex(bio->second, 32, bytes))
            return false;
        return catalog.Resolve(character, entry, error);
    }
    bool Ready(Buffer& out) const override {
        const uint8_t scene[] = {0xd7, 2, 0, 0, 0, 0, 0xe3, 2, 0xee, 2, 3, 0};
        Buffer reply;
        if (!reply.Append(ByteView(scene, sizeof(scene))))
            return false;
        out = std::move(reply);
        return true;
    }
    bool Signal(Buffer& out) const override {
        Buffer reply;
        if (!reply.U16(3))
            return false;
        out = std::move(reply);
        return true;
    }
    bool Created(const Guid& account, const Character& character, Buffer& out) const override {
        return ValidateSubmit(character) && CreatedReply(account, character, catalog, out);
    }
};
} // namespace
bool CreateCharacterFlow(CharacterCatalog catalog, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    error = "Invalid HoT creation catalog or starting location";
    if (catalog.RecipeCount() != 54 || initial.map != 15 || !std::isfinite(initial.x * 32) ||
        !std::isfinite(initial.y * 32) || !std::isfinite(initial.z * 32))
        return false;
    std::unique_ptr<HotCreate> flow(new HotCreate);
    flow->catalog = std::move(catalog);
    flow->initial = initial;
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.Load(dataRoot + L"/49284/construction.bin", error))
        return false;
    return CreateCharacterFlow(std::move(catalog), initial, out, error);
}
} // namespace hot49284
} // namespace glide
