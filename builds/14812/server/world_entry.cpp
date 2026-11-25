#include "world_entry.h"
#include "glide/files.h"
#include <cmath>
#include <utility>
namespace glide {
namespace july14812 {
bool NativePosition(float x, float y, float z, std::array<float, 3>& out) {
    const std::array<float, 3> value{{x * 32, y * 32, z == 0 ? 0 : -z * 32}};
    for (float coordinate : value)
        if (!std::isfinite(coordinate))
            return false;
    out = value;
    return true;
}
bool ValidMapEntry(const MapEntry& e) {
    std::wstring name;
    if (!e.player || !e.map || e.instance > 1 || e.name.find('\0') != std::string::npos || !Utf8ToWide(e.name, name) ||
        name.empty() || name.size() > 19)
        return false;
    for (float value : e.position)
        if (!std::isfinite(value))
            return false;
    for (float value : e.bounds)
        if (!std::isfinite(value))
            return false;
    return e.bounds[0] < e.bounds[2] && e.bounds[1] < e.bounds[3] && e.position[0] >= e.bounds[0] &&
           e.position[0] <= e.bounds[2] && e.position[1] >= e.bounds[1] && e.position[1] <= e.bounds[3];
}
bool EncodeMapEntry(const MapEntry& e, MapEntryStep step, Buffer& out) {
    if (!ValidMapEntry(e))
        return false;
    Buffer result;
    switch (step) {
    case MapEntryStep::Identity:
        if (!result.U16(466) || !result.Append(ByteView(e.missionState.data(), 4)) || !result.U16(467) ||
            !result.Utf16Z(e.name.c_str(), 19) || !result.U16(480) || !result.VarU32(e.player) ||
            !result.VarU32(e.map) || !result.VarU32(e.homeWorld) || !result.VarU32(e.worldClass) || !result.U8(0) ||
            !result.U8(e.instance))
            return false;
        break;
    case MapEntryStep::Spawn:
        if (!result.U16(464) || !result.U8(0) || !result.U16(484))
            return false;
        for (float value : e.position)
            if (!result.F32(value))
                return false;
        if (!result.U16(475) || !result.U16(e.previousMap) || !result.U16(479))
            return false;
        break;
    case MapEntryStep::Load:
        if (!result.U16(477) || !result.F32(e.position[0]) || !result.F32(e.position[1]))
            return false;
        for (float value : e.bounds)
            if (!result.F32(value))
                return false;
        if (!result.U16(e.map) || !result.U8(e.heading))
            return false;
        break;
    case MapEntryStep::Begin:
        if (!result.U16(465))
            return false;
        break;
    default:
        return false;
    }
    out = std::move(result);
    return true;
}
} // namespace july14812
} // namespace glide
