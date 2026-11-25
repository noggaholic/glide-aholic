#pragma once
#include "character_catalog.h"
#include "map_geometry.h"
#include "world_environment.h"
#include "game_wire.h"

namespace glide {
namespace june14505 {
bool WorldConstructionSteps(const BootstrapContext& context, const CharacterCatalog& characters,
                            const MapGeometryCatalog& maps, uint8_t player, uint16_t agent, uint16_t previousMap,
                            const std::array<uint32_t, 3>& achievements, const WorldDiscovery& discovery,
                            std::vector<BootstrapStep>& out);
}
} // namespace glide
