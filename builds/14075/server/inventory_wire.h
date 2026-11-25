#pragma once
#include "glide/buffer.h"
#include <array>

namespace glide {
namespace april14075 {
struct InventoryEquipRequest {
    uint32_t item = 0, slot = 0;
    size_t consumed = 0;
};
struct InventoryMoveRequest {
    uint32_t item = 0;
    uint8_t quantity = 0, location = 0, slot = 0;
    size_t consumed = 0;
};
DecodeStatus DecodeInventoryMoveRequest(ByteView input, InventoryMoveRequest& out);
// Client->server C55: two compact uint32 fields. Ownership, item restrictions
// and inventory mutation belong to the stateful inventory controller.
DecodeStatus DecodeInventoryEquipRequest(ByteView input, InventoryEquipRequest& out);
DecodeStatus DecodeInventoryUnequipRequest(ByteView input, InventoryEquipRequest& out);
// S100/S113 remove and release the native object. Re-report it with S321/S332
// before placing it in inventory (S99) or equipment (S98).
bool InventoryItemMove(uint16_t agent, uint32_t instance, uint8_t location, uint8_t slot, Buffer& out);
bool InventoryItemRemove(uint16_t agent, uint32_t instance, Buffer& out);
bool InventoryEquipmentRemove(uint16_t agent, uint32_t instance, Buffer& out);
bool InventoryArmorReport(uint32_t instance, uint32_t definition, const std::array<uint16_t, 4>& dyes, Buffer& out,
                          uint8_t flags = 0);
// April S332 -> CItemWeapon::Report: base item, two upgrade references,
// one byte and the final uint32 state. Starter weapons use the native defaults.
bool InventoryWeaponReport(uint32_t instance, uint32_t definition, Buffer& out, uint8_t flags = 0);
bool InventoryEquipmentAttach(uint16_t agent, uint32_t instance, uint8_t slot, Buffer& out);
// Server->client S322, verified against the original CItemBag receiver.
// The final byte is carried unchanged; its meaning is not inferred as capacity.
bool InventoryBagReport(uint32_t instance, uint32_t bagDataId, uint8_t flags, uint8_t bagState, Buffer& out);
// S106 sets the available bag slots. S105 places/removes a bag at its own
// inventory location (5), separate from the 59 equipment slots (location 2).
bool InventoryBagSlots(uint16_t agent, uint8_t count, Buffer& out);
bool InventoryBagSlot(uint16_t agent, uint32_t slot, uint32_t instance, Buffer& out);
bool InitialInventoryBag(uint8_t player, uint16_t agent, Buffer& out);
} // namespace april14075
} // namespace glide
