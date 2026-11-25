#pragma once
#include "glide/buffer.h"
#include <array>
#include <vector>

namespace glide {
namespace june14505 {
bool ProfessionState(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out);
// Initial exploration state: empty inventory/skills/pets, then finalize and player control.
bool OwnedManagers(uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel, uint32_t coins,
                   const std::array<uint32_t, 3>& achievementTotals, Buffer& out, ByteView equipment = ByteView());
bool LearnWeaponSkills(const std::vector<uint32_t>& skills, Buffer& out);
bool LearnWeaponSkillsForBuild(unsigned build, const std::vector<uint32_t>& skills, Buffer& out);
bool InitialElementalTraitsForBuild(unsigned build, uint8_t profession, unsigned level, Buffer& out);
bool RefreshEmptySkillbar(uint16_t agent, Buffer& out);
bool PeerManagers(uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out,
                  ByteView equipment = ByteView());
bool ProfessionStateForBuild(unsigned build, uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out);
bool OwnedManagersForBuild(unsigned build, uint8_t player, uint16_t agent, uint8_t profession, unsigned effectiveLevel,
                           uint32_t coins, const std::array<uint32_t, 3>& achievementTotals, Buffer& out,
                           ByteView equipment = ByteView(), uint8_t inventoryBagSlots = 0);
bool PeerManagersForBuild(unsigned build, uint16_t agent, uint8_t profession, unsigned effectiveLevel, Buffer& out,
                          ByteView equipment = ByteView());
} // namespace june14505
} // namespace glide
