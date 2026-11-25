#pragma once
#include "../../14505/server/mission.h"

namespace glide {
namespace april14075 {
using MissionMessage = june14505::MissionMessage;
using MissionAction = june14505::MissionAction;
DecodeStatus DecodeMission(ByteView input, MissionMessage& out);
bool MissionIdentity(const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld, uint32_t worldClass,
                     bool instance, Buffer& out);
bool MissionSpawn(const SpawnPoint& point, uint16_t previousMap, Buffer& out);
bool MissionLoad(const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading, Buffer& out);
bool MissionPrepare(Buffer& out);
bool MissionBegin(Buffer& out);
} // namespace april14075
} // namespace glide
