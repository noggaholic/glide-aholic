#pragma once
#include "character_catalog.h"
#include "map_catalog.h"
#include "glide/profile/game_bootstrap.h"
namespace glide {
namespace hot49284 {
bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out);
// itemBase is a reserved session range, never an ItemDef or recycled player slot.
bool WorldConstructionSteps(const BootstrapContext&, const CharacterCatalog&, const MapCatalog&, uint32_t itemBase,
                            std::vector<BootstrapStep>& out);
} // namespace hot49284
} // namespace glide
