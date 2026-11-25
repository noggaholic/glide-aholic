#include "world_managers.h"

namespace glide {
namespace april14075 {
bool ProfessionState(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out) {
    return june14505::ProfessionStateForBuild(14075, agent, profession, effectiveLevel, out);
}
bool OwnedManagers(uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel, uint32_t coins,
                   const std::array<uint32_t, 3>& achievementTotals, Buffer& out, ByteView equipment,
                   uint8_t inventoryBagSlots) {
    return june14505::OwnedManagersForBuild(14075, player, agent, profession, effectiveLevel, coins, achievementTotals,
                                            out, equipment, inventoryBagSlots);
}
bool PeerManagers(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out) {
    return june14505::PeerManagersForBuild(14075, agent, profession, effectiveLevel, out);
}
} // namespace april14075
} // namespace glide
