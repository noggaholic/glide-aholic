#include "world_entry.h"
#include <cmath>
#include <utility>
namespace glide {
namespace hot49284 {
bool NativePosition(float x, float y, float z, std::array<float, 3>& out) {
    const std::array<float, 3> value{{x * 32, y * 32, z == 0 ? 0 : -z * 32}};
    for (float coordinate : value)
        if (!std::isfinite(coordinate))
            return false;
    out = value;
    return true;
}
bool EncodeMapEntry(const MapEntry& entry, MapEntryStep step, Buffer& out) {
    if (!entry.player || !entry.map || entry.instance > 1 || entry.name.empty() ||
        entry.name.find('\0') != std::string::npos)
        return false;
    for (float value : entry.position)
        if (!std::isfinite(value))
            return false;
    for (float value : entry.bounds)
        if (!std::isfinite(value))
            return false;
    const auto& p = entry.position;
    const auto& b = entry.bounds;
    if (b[0] >= b[2] || b[1] >= b[3] || p[0] < b[0] || p[0] > b[2] || p[1] < b[1] || p[1] > b[3])
        return false;
    Buffer name, result;
    if (!name.Utf16Z(entry.name.c_str(), 19))
        return false;
    // Keep the world-entry identifiers consistent with the session assignment.
    switch (step) {
    case MapEntryStep::Identity:
        if (!result.U16(727) || !result.Append(ByteView(entry.missionState.data(), 4)) || !result.U16(728) ||
            !result.Append(name.View()) || !result.U16(752) || !result.VarU32(entry.player) ||
            !result.VarU32(entry.map) || !result.VarU32(entry.homeWorld) || !result.VarU32(entry.worldClass) ||
            !result.U8(0) || !result.U8(entry.instance))
            return false;
        break;
    case MapEntryStep::Spawn:
        if (!result.U16(725) || !result.U8(0) || !result.U16(755))
            return false;
        for (float value : p)
            if (!result.F32(value))
                return false;
        if (!result.U16(746) || !result.U16(entry.previousMap) || !result.U16(751))
            return false;
        break;
    case MapEntryStep::Load:
        if (!result.U16(749))
            return false;
        for (float value : p)
            if (!result.F32(value))
                return false;
        for (float value : b)
            if (!result.F32(value))
                return false;
        if (!result.U16(entry.map) || !result.U8(entry.heading))
            return false;
        break;
    case MapEntryStep::Begin:
        if (!result.U16(726))
            return false;
        break;
    default:
        return false;
    }
    out = std::move(result);
    return true;
}
} // namespace hot49284
} // namespace glide
