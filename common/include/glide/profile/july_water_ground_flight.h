#pragma once

namespace glide {
namespace beta {
struct JulyWaterGroundFlight {
    static bool Supports(unsigned skill) {
        return skill == 1155;
    }
    enum : unsigned { LaunchMs = 900, Speed = 600, Mode = 2 };
};
} // namespace beta
} // namespace glide
