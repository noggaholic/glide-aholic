#pragma once
#include "equipment_catalog.h"
#include "glide/profile/starter_inventory_state.h"
#include "glide/profile/world_actions.h"

namespace glide {
namespace china167409 {
using InventoryState = StarterInventoryState<68>;
bool InitializeInventory(const EquipmentCatalog& catalog, Character& character, std::string& error);
bool LoadInventoryState(const Character& saved, const EquipmentCatalog& catalog, InventoryState& out,
                        std::string& error);
bool CreateInventoryActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WorldActions>& out, std::string& error);
} // namespace china167409
} // namespace glide
