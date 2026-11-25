#pragma once

namespace glide {
namespace beta {
struct WaterGroundFlight {
    static bool Supports(unsigned build, unsigned skill) {
        return (build == 14075 || build == 14422 || build == 14505) && skill == 1155;
    }
    enum : unsigned { LaunchMs = 900, Speed = 600, Mode = 2 };
};
} // namespace beta
} // namespace glide
