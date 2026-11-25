#include "inventory_wire.h"
#include "../../14505/server/world_actor.h"
#include <utility>

namespace glide {
namespace april14075 {
DecodeStatus DecodeInventoryMoveRequest(ByteView input, InventoryMoveRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id = 0;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 31)
        return DecodeStatus::Unknown;
    InventoryMoveRequest next;
    const auto status = reader.VarU32(next.item);
    if (status != DecodeStatus::Complete)
        return status;
    if (!reader.U8(next.quantity) || !reader.U8(next.location) || !reader.U8(next.slot))
        return DecodeStatus::Incomplete;
    if (!next.item || !next.quantity || (next.location != 3 && next.location != 4 && next.location != 6))
        return DecodeStatus::Malformed;
    next.consumed = reader.Position();
    out = next;
    return DecodeStatus::Complete;
}
DecodeStatus DecodeInventoryUnequipRequest(ByteView input, InventoryEquipRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id = 0;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 56)
        return DecodeStatus::Unknown;
    InventoryEquipRequest next;
    auto status = reader.VarU32(next.item);
    if (status != DecodeStatus::Complete)
        return status;
    status = reader.VarU32(next.slot);
    if (status != DecodeStatus::Complete)
        return status;
    if (!next.item || next.slot >= 160)
        return DecodeStatus::Malformed;
    next.consumed = reader.Position();
    out = next;
    return DecodeStatus::Complete;
}
bool InventoryItemMove(uint16_t agent, uint32_t instance, uint8_t location, uint8_t slot, Buffer& out) {
    if (!agent || !instance || location != 3 || slot >= 160)
        return false;
    Buffer next;
    return next.U16(99) && next.VarU32(agent) && next.VarU32(instance) && next.U8(location) && next.U8(slot) &&
           out.Append(next.View());
}
bool InventoryItemRemove(uint16_t agent, uint32_t instance, Buffer& out) {
    if (!agent || !instance)
        return false;
    Buffer next;
    return next.U16(100) && next.VarU32(agent) && next.VarU32(instance) && out.Append(next.View());
}
bool InventoryEquipmentRemove(uint16_t agent, uint32_t instance, Buffer& out) {
    if (!agent || !instance)
        return false;
    Buffer next;
    return next.U16(113) && next.VarU32(agent) && next.VarU32(instance) && out.Append(next.View());
}
bool InventoryArmorReport(uint32_t instance, uint32_t definition, const std::array<uint16_t, 4>& dyes, Buffer& out,
                          uint8_t flags) {
    if (!instance || !definition)
        return false;
    Buffer next;
    if (!next.U16(321) || !next.VarU32(instance) || !next.VarU32(definition) || !next.U8(flags) || !next.VarU32(0))
        return false;
    for (uint16_t dye : dyes)
        if (!next.U16(dye))
            return false;
    return next.VarU32(0) && out.Append(next.View());
}
bool InventoryWeaponReport(uint32_t instance, uint32_t definition, Buffer& out, uint8_t flags) {
    if (!instance || !definition)
        return false;
    Buffer next;
    return next.U16(332) && next.VarU32(instance) && next.VarU32(definition) && next.U8(flags) && next.VarU32(0) &&
           next.VarU32(0) && next.U8(0) && next.VarU32(0) && out.Append(next.View());
}
bool InventoryEquipmentAttach(uint16_t agent, uint32_t instance, uint8_t slot, Buffer& out) {
    if (!agent || !instance || slot >= 59)
        return false;
    Buffer next;
    return next.U16(98) && next.VarU32(agent) && next.VarU32(instance) && next.U8(slot) && out.Append(next.View());
}
DecodeStatus DecodeInventoryEquipRequest(ByteView input, InventoryEquipRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id = 0;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 0x0037)
        return DecodeStatus::Unknown;
    InventoryEquipRequest next;
    auto status = reader.VarU32(next.item);
    if (status != DecodeStatus::Complete)
        return status;
    status = reader.VarU32(next.slot);
    if (status != DecodeStatus::Complete)
        return status;
    // The native inventory attach implementation rejects CHAR_EQUIP_SLOTS(59).
    if (!next.item || next.slot >= 59)
        return DecodeStatus::Malformed;
    next.consumed = reader.Position();
    out = next;
    return DecodeStatus::Complete;
}
bool InventoryBagReport(uint32_t instance, uint32_t bagDataId, uint8_t flags, uint8_t bagState, Buffer& out) {
    if (!instance || !bagDataId)
        return false;
    Buffer next;
    if (!next.U16(322) || !next.VarU32(instance) || !next.VarU32(bagDataId) || !next.U8(flags) || !next.U8(bagState))
        return false;
    out = std::move(next);
    return true;
}
bool InventoryBagSlots(uint16_t agent, uint8_t count, Buffer& out) {
    if (!agent || count > 8)
        return false;
    Buffer next;
    if (!next.U16(106) || !next.VarU32(agent) || !next.U8(count))
        return false;
    out = std::move(next);
    return true;
}
bool InventoryBagSlot(uint16_t agent, uint32_t slot, uint32_t instance, Buffer& out) {
    if (!agent || slot >= 8)
        return false;
    Buffer next;
    // The original receiver accepts item zero to empty this bag slot.
    if (!next.U16(105) || !next.VarU32(agent) || !next.VarU32(slot) || !next.VarU32(instance))
        return false;
    out = std::move(next);
    return true;
}
bool InitialInventoryBag(uint8_t player, uint16_t agent, Buffer& out) {
    return june14505::InitialInventoryBagForBuild(14075, player, agent, out);
}
} // namespace april14075
} // namespace glide
