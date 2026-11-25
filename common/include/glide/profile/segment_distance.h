#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace glide {
// Point and segment endpoints share a coordinate system; zero length is a point.
inline bool SegmentDistanceSquared(const std::array<double, 3>& point, const std::array<double, 3>& start,
                                   const std::array<double, 3>& end, double& out) {
    double length = 0, projection = 0;
    std::array<double, 3> delta{}, relative{};
    for (size_t i = 0; i < 3; ++i) {
        if (!std::isfinite(point[i]) || !std::isfinite(start[i]) || !std::isfinite(end[i]))
            return false;
        delta[i] = end[i] - start[i];
        relative[i] = point[i] - start[i];
        length += delta[i] * delta[i];
        projection += relative[i] * delta[i];
    }
    if (!std::isfinite(length) || !std::isfinite(projection))
        return false;
    const double t = length ? (std::max)(0.0, (std::min)(1.0, projection / length)) : 0;
    out = 0;
    for (size_t i = 0; i < 3; ++i) {
        const double value = relative[i] - t * delta[i];
        out += value * value;
    }
    return std::isfinite(out);
}
} // namespace glide
