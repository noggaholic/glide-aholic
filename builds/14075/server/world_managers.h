#pragma once
#include "../../14505/server/world_managers.h"

namespace glide {
namespace april14075 {
bool ProfessionState(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out);
bool OwnedManagers(uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel, uint32_t coins,
                   const std::array<uint32_t, 3>& achievementTotals, Buffer& out, ByteView equipment = ByteView(),
                   uint8_t inventoryBagSlots = 0);
bool PeerManagers(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out);
} // namespace april14075
} // namespace glide
