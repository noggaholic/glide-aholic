#pragma once
#include "world_entry.h"
#include "auth_response.h"
#include "glide/profile/modern_health_maximum.h"
namespace glide {
namespace hot49284 {
struct WorldActor {
    uint16_t agent = 257;
    uint32_t sequence = 1, itemBase = 0;
    std::array<float, 3> movement{{256, 2, 70}}, personality{{1, 0, 0}};
    float radius = 24, health = modern::kHotMaximumHealth, healthRecovery = 0;
    // -1 preserves the full-health default for directly constructed actors.
    float currentHealth = -1;
    std::array<std::array<uint16_t, 3>, 11> stats{};
    std::array<int, 12> inventoryLocations{};
    std::array<bool, 12> inventoryBound{};
    uint32_t endurance = 100, inventorySlots = 160, professionMaximum = 8, professionCurrent = 8;
    uint8_t attunement = 0;
    std::array<uint8_t, 2> legends{{0, 0}};
    WorldActor();
};
// Disjoint runtime instance IDs; content IDs come from HoT's original loadouts.
bool BindActor(WorldActor&, uint8_t player, uint32_t session);
bool EncodeWorldActor(const MapEntry&, const RosterEntry&, const WorldActor&, bool remote, Buffer&);
bool ActivateWorldActor(const MapEntry&, uint16_t agent, uint32_t sequence, bool remote, Buffer&);
bool AnnouncePeer(const MapEntry&, const RosterEntry&, const WorldActor&, Buffer& out);
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace hot49284
} // namespace glide
