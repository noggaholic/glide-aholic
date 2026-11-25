#pragma once
#include "glide/profile/world_actions.h"
#include "glide/character.h"
#include "world_loadout.h"
#include <memory>
namespace glide {
namespace hot49284 {
class WeaponActions : public WorldActions {
  public:
    virtual void SetEquipment(uint8_t equipped) = 0;
};
std::unique_ptr<WorldActions> CreateWorldActions(uint16_t ownedAgent);
std::unique_ptr<WeaponActions> CreateWeaponActions(uint16_t ownedAgent, const Character& saved,
                                                   const Hot49284Loadout& recipe, uint8_t equipped, std::string& error);
} // namespace hot49284
} // namespace glide
