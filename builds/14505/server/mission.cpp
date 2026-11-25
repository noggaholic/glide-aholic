#include "mission.h"
#include "glide/files.h"
#include <cmath>

namespace glide {
namespace june14505 {
namespace {
struct Protocol {
    uint16_t state, name, identity, loading, spawn, previous, spawnComplete, load, prepare, view, begin;
};
const Protocol april = {0x0176, 0x0177, 0x0186, 0x0173, 0x018d, 0x0187, 0x0185, 0x0183, 0x017e, 0x0184, 0x0175};
const Protocol june = {0x01d2, 0x01d3, 0x01e0, 0x01d0, 0x01e4, 0x01db, 0x01df, 0x01dd, 0x01d8, 0x01de, 0x01d1};
const Protocol* Wire(unsigned build) {
    return build == 14075 ? &april : (build == 14422 || build == 14505) ? &june : nullptr;
}
} // namespace
DecodeStatus DecodeMission(ByteView input, MissionMessage& out) {
    return DecodeMissionForBuild(input, 14505, out);
}
DecodeStatus DecodeMissionForBuild(ByteView input, unsigned build, MissionMessage& out) {
    if (!Wire(build))
        return DecodeStatus::Unknown;
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id, value;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (build == 14075) {
        const uint16_t ids[][2] = {{1, 1},           {2, 2},           {0x0075, 0x009b}, {0x0074, 0x009a},
                                   {0x008e, 0x000c}, {0x006a, 0x008f}, {0x0071, 0x0097}, {0x006d, 0x0092}};
        uint16_t canonical = UINT16_MAX;
        for (const auto& pair : ids)
            if (pair[0] == id) {
                canonical = pair[1];
                break;
            }
        id = canonical;
    }
    MissionMessage message;
    switch (id) {
    case 1:
        message.action = MissionAction::Close;
        break;
    case 2: {
        message.action = MissionAction::Heartbeat;
        const auto status = reader.VarU32(message.value);
        if (status != DecodeStatus::Complete)
            return status;
        break;
    }
    case 0x009b: {
        message.action = MissionAction::Identity;
        std::wstring login;
        for (;;) {
            if (login.size() >= 64)
                return DecodeStatus::Malformed;
            if (!reader.U16(value))
                return DecodeStatus::Incomplete;
            if (!value)
                break;
            login += static_cast<wchar_t>(value);
        }
        if (!WideToUtf8(login, message.login))
            return DecodeStatus::Malformed;
        if (!reader.U16(value))
            return DecodeStatus::Incomplete;
        message.value = value;
        break;
    }
    case 0x009a:
    case 0x000c:
        message.action = id == 0x009a ? MissionAction::MapAccepted : MissionAction::AgentReady;
        if (!reader.U16(value))
            return DecodeStatus::Incomplete;
        message.value = value;
        break;
    case 0x008f:
        message.action = MissionAction::SpawnAccepted;
        break;
    case 0x0097:
        message.action = MissionAction::MapLoaded;
        break;
    case 0x0092:
        message.action = MissionAction::WorldBuilt;
        break;
    case 0x0091:
        message.action = MissionAction::Ready;
        break;
    default:
        return DecodeStatus::Unknown;
    }
    message.consumed = reader.Position();
    out = std::move(message);
    return DecodeStatus::Complete;
}
bool MissionIdentity(const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld, uint32_t worldClass,
                     bool instance, Buffer& out) {
    return MissionIdentityForBuild(14505, name, player, map, homeWorld, worldClass, instance, out);
}
bool MissionIdentityForBuild(unsigned build, const std::string& name, uint32_t player, uint16_t map, uint32_t homeWorld,
                             uint32_t worldClass, bool instance, Buffer& out) {
    const auto* wire = Wire(build);
    if (!wire || !player || !map || name.empty())
        return false;
    Buffer result;
    if (!result.U16(wire->state) || !result.U32(255) || !result.U16(wire->name) || !result.Utf16Z(name.c_str(), 19) ||
        !result.U16(wire->identity) || !result.VarU32(player) || !result.VarU32(map) || !result.VarU32(homeWorld) ||
        !result.VarU32(worldClass) || !result.U8(0) || !result.U8(instance ? 1 : 0))
        return false;
    out = std::move(result);
    return true;
}
namespace {
bool Position(const SpawnPoint& point, float (&world)[3]) {
    if (!point.map || point.map > 65535)
        return false;
    world[0] = point.x * 32;
    world[1] = point.y * 32;
    world[2] = point.z == 0 ? 0 : -point.z * 32;
    return std::isfinite(world[0]) && std::isfinite(world[1]) && std::isfinite(world[2]);
}
} // namespace
bool MissionSpawn(const SpawnPoint& point, uint16_t previousMap, Buffer& out) {
    return MissionSpawnForBuild(14505, point, previousMap, out);
}
bool MissionSpawnForBuild(unsigned build, const SpawnPoint& point, uint16_t previousMap, Buffer& out) {
    const auto* wire = Wire(build);
    if (!wire)
        return false;
    float world[3];
    if (!Position(point, world))
        return false;
    Buffer result;
    if (!result.U16(wire->loading) || !result.U8(0) || !result.U16(wire->spawn))
        return false;
    for (float value : world)
        if (!result.F32(value))
            return false;
    if (!result.U16(wire->previous) || !result.U16(previousMap) || !result.U16(wire->spawnComplete))
        return false;
    out = std::move(result);
    return true;
}
bool MissionLoad(const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading, Buffer& out) {
    return MissionLoadForBuild(14505, point, bounds, heading, out);
}
bool MissionLoadForBuild(unsigned build, const SpawnPoint& point, const std::array<float, 4>& bounds, uint8_t heading,
                         Buffer& out) {
    const auto* wire = Wire(build);
    if (!wire)
        return false;
    float world[3];
    if (!Position(point, world))
        return false;
    for (float value : bounds)
        if (!std::isfinite(value))
            return false;
    if (bounds[0] >= bounds[2] || bounds[1] >= bounds[3])
        return false;
    Buffer result;
    if (!result.U16(wire->load) || !result.F32(world[0]) || !result.F32(world[1]))
        return false;
    for (float value : bounds)
        if (!result.F32(value))
            return false;
    if (!result.U16(static_cast<uint16_t>(point.map)) || !result.U8(heading))
        return false;
    out = std::move(result);
    return true;
}
bool MissionPrepare(Buffer& out) {
    return MissionPrepareForBuild(14505, out);
}
bool MissionPrepareForBuild(unsigned build, Buffer& out) {
    const auto* wire = Wire(build);
    if (!wire)
        return false;
    Buffer result;
    if (!result.U16(wire->prepare) || !result.U16(wire->view))
        return false;
    out = std::move(result);
    return true;
}
bool MissionBegin(Buffer& out) {
    return MissionBeginForBuild(14505, out);
}
bool MissionBeginForBuild(unsigned build, Buffer& out) {
    const auto* wire = Wire(build);
    if (!wire)
        return false;
    Buffer result;
    if (!result.U16(wire->begin))
        return false;
    out = std::move(result);
    return true;
}
} // namespace june14505
} // namespace glide
