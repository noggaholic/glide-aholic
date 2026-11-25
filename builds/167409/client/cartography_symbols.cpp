#include "cartography_symbols.h"
namespace glide {
namespace china167409 {
namespace {} // namespace
const std::vector<StartupSymbol>& CartographySymbols() {
    static const std::vector<StartupSymbol> result = {{"ChinaWorldMapTransform", {"ChinaWorldMapTransform"}},
                                                      {"ChinaWorldMapTransformRect", {"ChinaWorldMapTransformRect"}},
                                                      {"ChinaWorldMapInverse", {"ChinaWorldMapInverse"}}};
    return result;
}
} // namespace china167409
} // namespace glide
