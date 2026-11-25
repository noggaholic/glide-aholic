#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
namespace glide {
namespace july14812 {
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
bool NativePosition(float x, float y, float z, std::array<float, 3>&);
bool ValidMapEntry(const MapEntry&);
// July's Load carries two position floats; Spawn carries all three.
bool EncodeMapEntry(const MapEntry&, MapEntryStep, Buffer&);
} // namespace july14812
} // namespace glide
