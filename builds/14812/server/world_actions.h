#pragma once
#include "glide/profile/world_actions.h"
#include "character_catalog.h"
#include <memory>
namespace glide {
namespace july14812 {
class WeaponActions : public WorldActions {
  public:
    virtual void SetWeaponEquipped(bool equipped) = 0;
    virtual void SetAquaticWeaponEquipped(bool equipped) = 0;
};
std::unique_ptr<WeaponActions> CreateWorldActions(uint16_t ownedAgent);
std::unique_ptr<WeaponActions> CreateWorldActions(uint16_t ownedAgent, const Character&, const CharacterCatalog&);
// Reserved local projectile range1025..1278, separate from character Agents257..510.
uint16_t MissileAgent(uint16_t ownedAgent);
} // namespace july14812
} // namespace glide
