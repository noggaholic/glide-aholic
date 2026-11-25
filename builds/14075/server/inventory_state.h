#pragma once
#include "character_catalog.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/starter_inventory_state.h"
#include "../../14505/server/world_skills.h"
#include <array>

namespace glide {
namespace april14075 {
// Six original armor positions and two original weapons, sorted by native slot.
// -2 absent in the recipe, -1 equipped, 0..19 in the original General bag.
struct InventoryState : StarterInventoryState<8> {
    static bool Load(const Character& saved, const june14505::RosterCharacter& original, InventoryState& out,
                     std::string& error);
};
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error);
bool InitialInventoryEquipment(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                               Buffer& out, bool privateContents = true);
bool CreateInventoryActions(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                            std::unique_ptr<WorldActions>& out, std::string& error,
                            std::unique_ptr<june14505::WorldSkills> skills = nullptr);
} // namespace april14075
} // namespace glide
