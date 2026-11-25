#pragma once
#include "character_catalog.h"
#include "world_skills.h"
#include "glide/profile/starter_inventory_state.h"

namespace glide {
namespace june14505 {
using InventoryState = StarterInventoryState<8>;
enum class InventoryRequestKind { Move, Equip, Unequip };
struct InventoryRequest {
    InventoryRequestKind kind = InventoryRequestKind::Move;
    uint32_t item = 0, slot = 0;
    uint8_t quantity = 1, location = 3;
    size_t consumed = 0;
};
DecodeStatus DecodeInventoryRequest(unsigned build, ByteView input, InventoryRequest& out);
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error);
bool LoadInventoryState(const Character& saved, const RosterCharacter& original, InventoryState& out,
                        std::string& error);
bool InitialInventoryEquipment(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                               Buffer& out, bool privateContents = true);
bool RestoreEquippedInventoryWeapons(const CharacterCatalog& catalog, const Character& saved, uint8_t player,
                                     uint16_t agent, Buffer& out);
bool CreateInventoryActions(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                            std::unique_ptr<WorldSkills> skills, std::unique_ptr<WorldActions>& out,
                            std::string& error);
} // namespace june14505
} // namespace glide
