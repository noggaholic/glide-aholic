#include "glide/profile/wire_map.h"
#include "glide/schema.h"

namespace glide {
namespace {
constexpr SchemaField U8{FieldKind::U8}, U16{FieldKind::U16}, U32{FieldKind::U32};
constexpr SchemaField Raw12{FieldKind::Raw, 12}, Raw16{FieldKind::Raw, 16};
constexpr SchemaField Var{FieldKind::Var}, Len8{FieldKind::Len8}, Len16{FieldKind::Len16};
constexpr SchemaField GuidField{FieldKind::Guid}, Utf16{FieldKind::Utf16Z};
template <size_t N> constexpr SchemaField Nest(const SchemaField (&children)[N]) {
    return {FieldKind::Nest, 0, children, N};
}
const SchemaField optionalVector[] = {Raw16};
const SchemaField locomotion[] = {Raw16, Var, Nest(optionalVector), Nest(optionalVector), U8, U8};
const SchemaField optionalGuid[] = {GuidField};
const SchemaField trajectory[] = {Var, Var, Nest(optionalGuid)};
const SchemaField m0002[] = {Var};
const SchemaField m0004[] = {U16, Var, U8, Nest(locomotion)};
const SchemaField m0005[] = {U16, Var, Var};
const SchemaField m0006[] = {U16, Var, U8, Var, Len8, U16};
const SchemaField m0007[] = {U16, Var, U8, Var, Len8, Nest(locomotion), U16, U16, Var};
const SchemaField m0009[] = {U16, Var, U8, Var, Len8, Nest(locomotion)};
const SchemaField m000a[] = {U16, Var, U8, Var, Len8, Nest(locomotion), Raw12, U32};
const SchemaField m000b[] = {U16, Var, U8, Var, Len8, Nest(locomotion), Raw12, Raw12, Nest(trajectory)};
const SchemaField m000d[] = {U16, Var, U8, Var, Len8, U32, U16, U16};
const SchemaField m000e[] = {U16, Var, U8, Var, Len8, U16, U16, U16};
const SchemaField m000f[] = {U16, Var, U8, Var, Len8, GuidField, U16, U16, U8};
const SchemaField m0010[] = {U16, Var, U8, U8, U32};
const SchemaField m0011[] = {U16};
const SchemaField m0012[] = {Var, Var, Raw12, Var, U8, U8, U8};
const SchemaField m0014[] = {U16, Var, U8, Var, Len8, U32, U32};
const SchemaField m0017[] = {U16, Var, U8, Var, Len8};
const SchemaField m0018[] = {U16, Var, GuidField, Raw12, U16, U8, Var, Len8};
const SchemaField m0019[] = {U16, Var, U8, GuidField};
const SchemaField m001a[] = {U16, Var, U8, Raw16, Var, Raw16, Raw16, U8, U8};
const SchemaField m0080[] = {U8};
const SchemaField blob[] = {Len16};
const SchemaField waypoint[] = {Var};
// The existing chat decoder reads a compact target; seven trailing bytes is only its maximum size.
const SchemaField chat[] = {Utf16, U8, Var, U8};
const SchemaField hello[] = {U8,  U8,  Var, U16, Utf16, Var, Utf16, U16, Var,   Var,  Var,
                             U16, Var, U16, U16, U16,   U16, U8,    U8,  Raw16, Utf16};
const SchemaField transferHello[] = {Utf16, Utf16, U8, U8};
const SchemaField missionControl[] = {U16};
const SchemaField inventoryMove[] = {Var, U8, U8, Var};
const SchemaField inventoryEquip[] = {Var, Var};
const SchemaField legendSelection[] = {U8, U8, U8};
const SchemaField inventoryUnequip[] = {Var, U8, Var};
// Retained DebugInfo report: small-sandbox SEND_SCHEMAS 0x0118, wire 0x0119 here.
// No reply; metric key, at most 32 compact values, raw context GUID, flags.
const SchemaField metricValue[] = {Var};
const SchemaField metricReport[] = {Var, {FieldKind::Array8, 32, metricValue, 1}, Raw16, U8};

struct MessageSchema {
    uint16_t canonical;
    GameAction action;
    const SchemaField* fields;
    size_t count;
};
template <size_t N>
constexpr MessageSchema Schema(uint16_t canonical, GameAction action, const SchemaField (&fields)[N]) {
    return {canonical, action, fields, N};
}
const MessageSchema schemas[] = {{192, GameAction::Inventory, legendSelection, 3},
                                 Schema(85, GameAction::Inventory, inventoryMove),
                                 Schema(171, GameAction::Inventory, inventoryEquip),
                                 Schema(172, GameAction::Inventory, inventoryEquip),
                                 Schema(176, GameAction::Inventory, inventoryUnequip),
                                 {0x0001, GameAction::Close, nullptr, 0},
                                 Schema(0x0002, GameAction::Heartbeat, m0002),
                                 Schema(0x0004, GameAction::Motion, m0004),
                                 Schema(0x0005, GameAction::Motion, m0005),
                                 Schema(0x0006, GameAction::Motion, m0006),
                                 Schema(0x0007, GameAction::Motion, m0007),
                                 Schema(0x0009, GameAction::Motion, m0009),
                                 Schema(0x000a, GameAction::Motion, m000a),
                                 Schema(0x000b, GameAction::Motion, m000b),
                                 Schema(0x000d, GameAction::Motion, m000d),
                                 Schema(0x000e, GameAction::Motion, m000e),
                                 Schema(0x000f, GameAction::Motion, m000f),
                                 Schema(0x0010, GameAction::Motion, m0010),
                                 Schema(0x0011, GameAction::Control, m0011),
                                 Schema(0x0012, GameAction::Position, m0012),
                                 Schema(0x0014, GameAction::Motion, m0014),
                                 Schema(0x0017, GameAction::Motion, m0017),
                                 Schema(0x0018, GameAction::Motion, m0018),
                                 Schema(0x0019, GameAction::Motion, m0019),
                                 Schema(0x001a, GameAction::Position, m001a),
                                 // Own empty C49 follows C128 when opening the native Build panel.
                                 {0x0031, GameAction::Control, nullptr, 0},
                                 // C128 contains one U8; opening the native Build panel emits it.
                                 // Frame it as control without changing specialization or training state.
                                 Schema(0x0080, GameAction::Control, m0080),
                                 Schema(0x00bf, GameAction::Control, blob),
                                 Schema(0x00ce, GameAction::Waypoint, waypoint),
                                 // Local action settings and selection each contain one compact value.
                                 // These do not activate a skill; the action command validates its target.
                                 Schema(0x00e0, GameAction::Control, m0002),
                                 Schema(0x00e1, GameAction::Control, m0002),
                                 Schema(0x00e2, GameAction::Control, m0002),
                                 Schema(0x00e3, GameAction::Control, m0002),
                                 Schema(0x00eb, GameAction::Control, m0002),
                                 // Native character-view notification: U16 page/state, no inventory mutation or reply.
                                 Schema(0x00f8, GameAction::Control, m0011),
                                 Schema(0x0105, GameAction::Chat, chat),
                                 Schema(0x0116, GameAction::Hello, hello),
                                 Schema(0x0118, GameAction::Control, metricReport),
                                 {0x0198, GameAction::Control, nullptr, 0},
                                 {0x0199, GameAction::MapContextReady, nullptr, 0},
                                 {0x019c, GameAction::MapLoaderReady, nullptr, 0},
                                 {0x01a3, GameAction::Control, nullptr, 0},
                                 {0x01a4, GameAction::MapLoadAccepted, nullptr, 0},
                                 Schema(0x01a7, GameAction::Control, missionControl),
                                 Schema(0x01a8, GameAction::TransferHello, transferHello)};
uint16_t Canonical(uint16_t wire) {
    if (wire == 0x00d0)
        return 0x00ce;
    if (wire == 0x0106)
        return 0x0105;
    if ((wire >= 0x0117 && wire < 0x0120) || (wire >= 0x019a && wire < 0x01bd))
        return static_cast<uint16_t>(wire - 1);
    return wire;
}
class Build204489WireMap : public WireMap {
  public:
    DecodeStatus DecodeGame(ByteView input, GameMessage& out) const override {
        if (!input.Valid())
            return DecodeStatus::Malformed;
        Reader reader(input);
        uint16_t wire;
        if (!reader.U16(wire))
            return DecodeStatus::Incomplete;
        const uint16_t canonical = Canonical(wire);
        for (const auto& schema : schemas)
            if (schema.canonical == canonical) {
                size_t size = 0;
                const auto status =
                    SchemaSize(schema.fields, schema.count, ByteView(input.data + 2, input.size - 2), size);
                if (status == DecodeStatus::Complete)
                    out = GameMessage{wire, schema.action, size + 2};
                return status;
            }
        return DecodeStatus::Unknown;
    }
    bool BarrierMatches(GameConfirmation expected, const GameMessage& received) const override {
        // Both greetings are valid on an initial connection or a map transfer in the existing client.
        switch (expected) {
        case GameConfirmation::Hello:
        case GameConfirmation::TransferHello:
            return received.action == GameAction::Hello || received.action == GameAction::TransferHello;
        case GameConfirmation::MapContextReady:
            return received.action == GameAction::MapContextReady;
        case GameConfirmation::MapLoadAccepted:
            return received.action == GameAction::MapLoadAccepted;
        case GameConfirmation::MapLoaderReady:
            return received.action == GameAction::MapLoaderReady;
        }
        return false;
    }
};
} // namespace
const WireMap& Build204489Wire() {
    static Build204489WireMap wire;
    return wire;
}
} // namespace glide
