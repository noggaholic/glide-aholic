#pragma once
#include "glide/character.h"
#include <array>

namespace glide {
namespace june14505 {
enum class MissionAction {
    Identity,
    MapAccepted,
    SpawnAccepted,
    MapLoaded,
    WorldBuilt,
    AgentReady,
    Close,
    Heartbeat,
    Ready
};
struct MissionMessage {
    MissionAction action = MissionAction::Ready;
    size_t consumed = 0;
    std::string login;
    uint32_t value = 0;
};
DecodeStatus DecodeMission(ByteView input, MissionMessage& out);
DecodeStatus DecodeMissionForBuild(ByteView input, unsigned build, MissionMessage& out);
bool MissionIdentity(const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld, uint32_t worldClass,
                     bool instance, Buffer& out);
// Positions use saved/server units; bounds are the original MapDef world units.
bool MissionSpawn(const SpawnPoint& point, uint16_t previousMap, Buffer& out);
bool MissionLoad(const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading, Buffer& out);
bool MissionPrepare(Buffer& out);
bool MissionBegin(Buffer& out);
bool MissionIdentityForBuild(unsigned build, const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld,
                             uint32_t worldClass, bool instance, Buffer& out);
bool MissionSpawnForBuild(unsigned build, const SpawnPoint& point, uint16_t previousMap, Buffer& out);
bool MissionLoadForBuild(unsigned build, const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading,
                         Buffer& out);
bool MissionPrepareForBuild(unsigned build, Buffer& out);
bool MissionBeginForBuild(unsigned build, Buffer& out);
} // namespace june14505
} // namespace glide
