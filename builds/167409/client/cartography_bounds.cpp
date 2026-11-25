#include "cartography.h"
#include <cmath>
namespace glide {
namespace china167409 {
bool CartographyUnavailable(const float* layout) {
    if (!layout)
        return false;
    const float width = layout[2] - layout[0], height = layout[3] - layout[1];
    return !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0;
}
void CartographyOrigin(const float* layout, float* output) {
    output[0] = std::isfinite(layout[0]) ? layout[0] : 0;
    output[1] = std::isfinite(layout[1]) ? layout[1] : 0;
}
} // namespace china167409
} // namespace glide
