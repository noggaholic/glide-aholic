#include "protocol.h"
#include "glide/schema.h"

namespace glide {
const WireMap& Build204489Wire();
namespace {
constexpr SchemaField U8{FieldKind::U8}, U16{FieldKind::U16};
constexpr SchemaField Var{FieldKind::Var}, Raw16{FieldKind::Raw, 16};
// Original China capacities include the terminating zero; SchemaSize excludes it.
constexpr SchemaField Text13{FieldKind::Utf16Z, 12}, Text16{FieldKind::Utf16Z, 15};
constexpr SchemaField Text64{FieldKind::Utf16Z, 63}, Text72{FieldKind::Utf16Z, 71};
constexpr SchemaField Text128{FieldKind::Utf16Z, 127}, Text200{FieldKind::Utf16Z, 199};
constexpr SchemaField Text256{FieldKind::Utf16Z, 255};
const SchemaField hello[] = {U8,  U8,  Var, U16, Text13, Var, Text128, U16, Var, Var,   Var,
                             U16, Var, U16, U16, U16,    U16, U16,     U8,  U8,  Raw16, Text256};
const SchemaField transferHello[] = {Text64, Text16, U8, U8};
const SchemaField unequip[] = {Var, U8, Var};
const SchemaField equip[] = {Var, Var};
const SchemaField legendSelection[] = {U8, U8, U8};
const SchemaField inventoryMove[] = {Var, U8, U8, Var};
const SchemaField chat[] = {Text200, U8, Var, U8};
const SchemaField missionFlags[] = {U8, U8};
const SchemaField compact[] = {Var};
const SchemaField byte[] = {U8};
// Original C235 is a window/page notification (U16); S235 is unrelated wardrobe data.
const SchemaField panel[] = {U16};
const SchemaField vector[] = {Raw16};
const SchemaField identity[] = {Var, Raw16, Text72};
const SchemaField controlBf[] = {Var, U8};
const SchemaField controlCe[] = {Var, U8, U8};
const SchemaField controlBlob[] = {{FieldKind::Len16}};
const SchemaField compactArrayElement[] = {Var};
const SchemaField postLoadControl[] = {Var, {FieldKind::Array8, 32, compactArrayElement, 1}, Raw16, U8};
struct MessageSchema {
    uint16_t wire;
    GameAction action;
    const SchemaField* fields;
    size_t count;
};
template <size_t N> constexpr MessageSchema Schema(uint16_t wire, GameAction action, const SchemaField (&fields)[N]) {
    return {wire, action, fields, N};
}
const MessageSchema schemas[] = {Schema(180, GameAction::Inventory, legendSelection),
                                 Schema(0x0052, GameAction::Inventory, inventoryMove),
                                 Schema(0x00a0, GameAction::Inventory, equip),
                                 Schema(0x00a1, GameAction::Inventory, equip),
                                 Schema(0x00a5, GameAction::Inventory, unequip),
                                 Schema(0x00b3, GameAction::Control, controlBlob),
                                 Schema(0x00bf, GameAction::Control, controlBf),
                                 Schema(0x00ce, GameAction::Control, controlCe),
                                 // Original local action settings: mode, target agent, and selected SkillDef.
                                 // These do not activate a skill; the audited C23 command carries the action.
                                 Schema(0x00d3, GameAction::Control, compact),
                                 Schema(0x00d4, GameAction::Control, compact),
                                 Schema(0x00d5, GameAction::Control, compact),
                                 // C214 reports the selected agent reference, or zero when cleared.
                                 Schema(0x00d6, GameAction::Control, compact),
                                 // Original C222 reports the locally selected character (VarU32).
                                 // C23 carries the skill's actual target; this notification needs no reply.
                                 Schema(0x00de, GameAction::Control, compact),
                                 Schema(0x00eb, GameAction::Control, panel),
                                 Schema(0x00f9, GameAction::Chat, chat),
                                 Schema(0x010a, GameAction::Hello, hello),
                                 Schema(0x010c, GameAction::Control, postLoadControl),
                                 Schema(0x0115, GameAction::Control, compact),
                                 Schema(0x0116, GameAction::Control, compact),
                                 {0x0180, GameAction::MapContextReady, nullptr, 0},
                                 {0x0183, GameAction::MapLoaderReady, nullptr, 0},
                                 {0x018a, GameAction::MapLoadAccepted, nullptr, 0},
                                 Schema(0x018d, GameAction::Control, missionFlags),
                                 Schema(0x018e, GameAction::TransferHello, transferHello),
                                 {0x0198, GameAction::Control, nullptr, 0},
                                 Schema(0x0199, GameAction::Control, byte),
                                 Schema(0x019a, GameAction::Control, byte),
                                 {0x019b, GameAction::Control, nullptr, 0},
                                 {0x019c, GameAction::Control, nullptr, 0},
                                 Schema(0x01a2, GameAction::Control, compact),
                                 Schema(0x01a3, GameAction::Control, byte),
                                 Schema(0x01a4, GameAction::Control, vector),
                                 Schema(0x01a7, GameAction::Control, identity),
                                 Schema(0x01a8, GameAction::Control, identity)};
bool SharedMotionId(uint16_t id) {
    switch (id) {
    case 0x0001:
    case 0x0002:
    case 0x0004:
    case 0x0005:
    case 0x0006:
    case 0x0007:
    case 0x0009:
    case 0x000a:
    case 0x000b:
    case 0x000d:
    case 0x000e:
    case 0x000f:
    case 0x0010:
    case 0x0011:
    case 0x0012:
    case 0x0014:
    case 0x0017:
    case 0x0018:
    case 0x0019:
    case 0x001a:
        return true;
    default:
        return false;
    }
}
bool HasMotionBlob(uint16_t id) {
    switch (id) {
    case 0x0006:
    case 0x0007:
    case 0x0009:
    case 0x000a:
    case 0x000b:
    case 0x000d:
    case 0x000e:
    case 0x000f:
    case 0x0014:
    case 0x0017:
        return true;
    default:
        return false;
    }
}
DecodeStatus MotionBlobBound(ByteView input) {
    Reader reader(input);
    uint16_t word;
    uint32_t compactValue;
    uint8_t byteValue;
    if (!reader.U16(word) || !reader.U16(word))
        return DecodeStatus::Incomplete;
    auto status = reader.VarU32(compactValue);
    if (status != DecodeStatus::Complete)
        return status;
    if (!reader.U8(byteValue))
        return DecodeStatus::Incomplete;
    status = reader.VarU32(compactValue);
    if (status != DecodeStatus::Complete)
        return status;
    if (!reader.U8(byteValue))
        return DecodeStatus::Incomplete;
    // Original compact-blob capacity is 0x100f6: one-byte length, maximum 246 bytes.
    return byteValue <= 246 ? DecodeStatus::Complete : DecodeStatus::Malformed;
}
class ChinaWire : public WireMap {
  public:
    DecodeStatus DecodeGame(ByteView input, GameMessage& out) const override {
        if (!input.Valid())
            return DecodeStatus::Malformed;
        Reader reader(input);
        uint16_t wire;
        if (!reader.U16(wire))
            return DecodeStatus::Incomplete;
        if (wire == 0x00b3) {
            uint16_t length;
            if (!reader.U16(length))
                return DecodeStatus::Incomplete;
            // Native field type 21 has a two-byte length and an 8064-byte capacity.
            if (length > 8064)
                return DecodeStatus::Malformed;
        }
        for (const auto& schema : schemas)
            if (schema.wire == wire) {
                size_t consumed = 0;
                const auto status =
                    SchemaSize(schema.fields, schema.count, ByteView(input.data + 2, input.size - 2), consumed);
                if (status == DecodeStatus::Complete)
                    out = GameMessage{wire, schema.action, consumed + 2};
                return status;
            }
        // Only the audited common movement family is shared. Modern ID aliases stay out.
        if (!SharedMotionId(wire))
            return DecodeStatus::Unknown;
        if (HasMotionBlob(wire)) {
            const auto status = MotionBlobBound(input);
            if (status != DecodeStatus::Complete)
                return status;
        }
        return Build204489Wire().DecodeGame(input, out);
    }
    bool BarrierMatches(GameConfirmation expected, const GameMessage& received) const override {
        switch (expected) {
        case GameConfirmation::Hello:
        case GameConfirmation::TransferHello:
            return (received.wireId == 0x010a && received.action == GameAction::Hello) ||
                   (received.wireId == 0x018e && received.action == GameAction::TransferHello);
        case GameConfirmation::MapContextReady:
            return received.wireId == 0x0180 && received.action == GameAction::MapContextReady;
        case GameConfirmation::MapLoadAccepted:
            return received.wireId == 0x018a && received.action == GameAction::MapLoadAccepted;
        case GameConfirmation::MapLoaderReady:
            return received.wireId == 0x0183 && received.action == GameAction::MapLoaderReady;
        }
        return false;
    }
};
} // namespace
const WireMap& China167409Wire() {
    static const ChinaWire wire;
    return wire;
}
} // namespace glide
