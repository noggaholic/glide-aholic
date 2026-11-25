#pragma once
#include "equipment_catalog.h"
#include "glide/profile/world_actions.h"

namespace glide {
namespace build204489 {
class WeaponController : public WorldActions {
  public:
    virtual void SetEquippedWeapons(const std::array<uint32_t, 68>& definitions) = 0;
};
bool CreateWeaponController(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WeaponController>& out, std::string& error);
bool CreateWeaponActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                         std::unique_ptr<WorldActions>& out, std::string& error);
} // namespace build204489
} // namespace glide
