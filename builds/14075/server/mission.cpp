#include "mission.h"

namespace glide {
namespace april14075 {
DecodeStatus DecodeMission(ByteView input, MissionMessage& out) {
    return june14505::DecodeMissionForBuild(input, 14075, out);
}
bool MissionIdentity(const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld, uint32_t worldClass,
                     bool instance, Buffer& out) {
    return june14505::MissionIdentityForBuild(14075, name, player, map, homeWorld, worldClass, instance, out);
}
bool MissionSpawn(const SpawnPoint& point, uint16_t previousMap, Buffer& out) {
    return june14505::MissionSpawnForBuild(14075, point, previousMap, out);
}
bool MissionLoad(const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading, Buffer& out) {
    return june14505::MissionLoadForBuild(14075, point, bounds, heading, out);
}
bool MissionPrepare(Buffer& out) {
    return june14505::MissionPrepareForBuild(14075, out);
}
bool MissionBegin(Buffer& out) {
    return june14505::MissionBeginForBuild(14075, out);
}
} // namespace april14075
} // namespace glide
