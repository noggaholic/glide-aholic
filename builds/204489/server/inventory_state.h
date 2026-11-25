#pragma once
#include "equipment_catalog.h"
#include "weapon_actions.h"

namespace glide {
namespace build204489 {
bool CreateInventoryActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WorldActions>& out, std::string& error);
}
} // namespace glide
