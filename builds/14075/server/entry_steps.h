#pragma once
#include "character_catalog.h"
#include "map_geometry.h"
#include "glide/profile/game_bootstrap.h"

namespace glide {
namespace april14075 {
bool WorldConstructionSteps(const BootstrapContext& context, const CharacterCatalog& characters,
                            const MapGeometryCatalog& maps, std::vector<BootstrapStep>& out);
}
} // namespace glide
