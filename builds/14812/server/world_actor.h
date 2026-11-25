#pragma once
#include "world_entry.h"
#include "character_catalog.h"
namespace glide {
namespace july14812 {
struct EquippedArmor {
    uint32_t instance = 0;
    uint8_t slot = 0, flags = 0;
};
struct EquippedWeapon {
    uint32_t instance = 0, item = 0, appearance = 0, upgrade = 0, reportValue = 0;
    uint8_t slot = 0, flags = 0, reportState = 0;
};
struct WorldActor {
    uint16_t agent = 257;
    uint32_t sequence = 1;
    uint32_t karma = 0;
    std::array<float, 3> movement{{256, 2, 70}}, personality{{1, 0, 0}};
    float radius = 24, health = 100, healthRecovery = 0, maximumHealth = 100;
    std::array<std::array<uint16_t, 3>, 8> stats{};
    uint32_t endurance = 100, professionMaximum = 8, professionCurrent = 8, inventorySlots = 160;
    std::array<EquippedArmor, 6> armor{};
    std::array<EquippedWeapon, 6> weapons{};
    uint8_t weaponSetPresent = 0, weaponSet = 0, professionStatePresent = 0, professionState = 0;
    std::vector<uint32_t> learnedSkills;
    std::vector<uint32_t> automaticTraits;
};
// Uses only July's saved appearance, complete palette and own level1 recipes.
bool PrepareActor(const Character&, const CharacterCatalog&, uint32_t sequence, WorldActor&);
// Atomic; relative instances1..31 become unique for this world connection.
bool BindActor(WorldActor&, uint8_t player, uint32_t session);
bool EncodeWorldActor(const MapEntry&, const RosterEntry&, const WorldActor&, bool remote, Buffer&);
bool ActivateWorldActor(const MapEntry&, uint16_t agent, uint32_t sequence, Buffer&);
bool AnnouncePeer(const MapEntry&, const RosterEntry&, const WorldActor&, Buffer&);
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer&);
} // namespace july14812
} // namespace glide
