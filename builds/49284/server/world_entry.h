#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
namespace glide {
namespace hot49284 {
// Coordinates and bounds are native game units, as in the retained HoT encoder.
struct MapEntry {
    uint32_t player = 0, homeWorld = 0, worldClass = 0;
    uint16_t map = 0, previousMap = 0;
    std::string name;
    std::array<uint8_t, 4> missionState{};
    uint8_t instance = 0, heading = 0;
    std::array<float, 3> position{};
    std::array<float, 4> bounds{};
};
enum class MapEntryStep { Identity, Spawn, Load, Begin };
// Shared storage uses 1/32 units and the opposite vertical direction.
bool NativePosition(float x, float y, float z, std::array<float, 3>& out);
// Atomic encoding only; the bootstrap must wait for each client confirmation.
bool EncodeMapEntry(const MapEntry&, MapEntryStep, Buffer& out);
} // namespace hot49284
} // namespace glide
