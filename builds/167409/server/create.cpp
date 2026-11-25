#include "create.h"
#include "protocol.h"
#include "glide/schema.h"
namespace glide {
const CreateFlow& Build204489Create();
namespace {
class ChinaCreate final : public CreateFlow {
  public:
    const SpawnPoint& InitialLocation() const override {
        return Build204489Create().InitialLocation();
    }
    bool ValidateSubmit(const Character& character) const override {
        return Build204489Create().ValidateSubmit(character);
    }
    DecodeStatus Decode(ByteView input, CreateMessage& out) const override {
        if (!input.Valid())
            return DecodeStatus::Malformed;
        Reader reader(input);
        uint16_t id;
        if (!reader.U16(id))
            return DecodeStatus::Incomplete;
        uint16_t modern = 0;
        if (id == 0x0185 || id == 0x0186) {
            const size_t prefix = id == 0x0186 ? 3 : 2;
            if (input.size < prefix)
                return DecodeStatus::Incomplete;
            const SchemaField name{FieldKind::Utf16Z, 19};
            size_t consumed = 0;
            const auto status = SchemaSize(&name, 1, ByteView(input.data + prefix, input.size - prefix), consumed);
            if (status != DecodeStatus::Complete)
                return status;
            modern = id == 0x0185 ? 0x01a0 : 0x01a1;
        } else if (id == 0x018e) {
            GameMessage validated;
            const auto status = China167409Wire().DecodeGame(input, validated);
            if (status != DecodeStatus::Complete)
                return status;
            modern = 0x01a9;
        } else if (id == 0x0194) {
            CreateMessage result;
            result.action = CreateAction::Complete;
            result.consumed = 2;
            out = std::move(result);
            return DecodeStatus::Complete;
        } else if (id == 0x0181 || id == 0x0182 || id == 0x0187 || id == 0x0188 || id == 0x018c || id == 0x018f ||
                   id == 0x0192 || id == 0x0193 || id == 0x019d || id == 0x019f || id == 0x01a0 || id == 0x01a1) {
            // Original group38 descriptors are empty for these creation/session controls.
            CreateMessage result;
            result.consumed = 2;
            out = std::move(result);
            return DecodeStatus::Complete;
        } else {
            GameMessage validated;
            const auto status = China167409Wire().DecodeGame(input, validated);
            if (status != DecodeStatus::Complete)
                return status;
            CreateMessage result;
            result.consumed = validated.consumed;
            result.action = validated.action == GameAction::Close ? CreateAction::Close : CreateAction::Control;
            out = std::move(result);
            return DecodeStatus::Complete;
        }
        Buffer common;
        if (!common.U16(modern) || !common.Append(ByteView(input.data + 2, input.size - 2)))
            return DecodeStatus::Malformed;
        CreateMessage result;
        const auto status = Build204489Create().Decode(common.View(), result);
        if (status != DecodeStatus::Complete)
            return status;
        out = std::move(result);
        return status;
    }
    bool Ready(Buffer& out) const override {
        const uint8_t bytes[] = {0xf0, 3, 0xff, 0, 0, 0, 0xfc, 3, 3, 0};
        Buffer result;
        if (!result.Append(ByteView(bytes, sizeof(bytes))))
            return false;
        out = std::move(result);
        return true;
    }
    bool Signal(Buffer& out) const override {
        return Build204489Create().Signal(out);
    }
    bool Created(const Guid& user, const Character& character, Buffer& out) const override {
        Buffer result;
        if (!Build204489Create().Created(user, character, result) || result.Size() < 2)
            return false;
        result.Data()[0] = 0xfa;
        result.Data()[1] = 3;
        out = std::move(result);
        return true;
    }
};
} // namespace
const CreateFlow& China167409Create() {
    static const ChinaCreate create;
    return create;
}
} // namespace glide
