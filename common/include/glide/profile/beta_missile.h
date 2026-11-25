#pragma once
#include "glide/buffer.h"
#include <array>
#include <cmath>
#include <cstring>
#include <utility>

namespace glide {
namespace beta {
inline bool MissileBuild(unsigned build) {
    return build == 14075 || build == 14422 || build == 14505 || build == 14812;
}
inline bool MissilePoint(const std::array<float, 3>& point) {
    for (float value : point)
        if (!std::isfinite(value))
            return false;
    return true;
}
// Original native layouts: April creation input 30 bytes, June/July 34.
// July alone has a token after the Agent creation pose (outside that input).
inline bool CreateMissile(unsigned build, uint16_t agent, uint32_t sequence, uint32_t identity, uint16_t owner,
                          uint32_t skill, const std::array<float, 3>& start, uint32_t token, Buffer& out) {
    if (!MissileBuild(build) || !agent || !owner || agent == owner || !identity || (identity & 0x80000000u) || !skill ||
        !MissilePoint(start))
        return false;
    std::array<uint8_t, 34> input{};
    const uint32_t owner32 = owner;
    const bool april = build == 14075;
    std::memcpy(input.data(), &identity, 4);
    std::memcpy(input.data() + 4, &owner32, 4);
    std::memcpy(input.data() + 8, &skill, 4);
    std::memcpy(input.data() + (april ? 12 : 16), start.data(), 12);
    std::memcpy(input.data() + (april ? 24 : 28), &token, 4);
    Buffer result;
    const uint8_t length = april ? 30 : 34;
    if (!result.U16(april ? 14 : 15) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x80) ||
        !result.VarU32(identity) || !result.U8(length) || !result.Append(ByteView(input.data(), length)) ||
        !result.U8(14) || !result.VarU32(identity) || !result.U8(0x10) || !result.U8(0) || !result.U8(0))
        return false;
    for (float value : start)
        if (!result.F32(value))
            return false;
    if (!result.F32(0) || (build == 14812 && !result.VarU32(token)))
        return false;
    if (!result.U8(0) || !result.U8(0) || !result.U8(0) || !result.U8(0) || !result.U16(0) || !result.U8(0))
        return false;
    out = std::move(result);
    return true;
}
inline bool MoveMissile(unsigned build, uint16_t agent, uint32_t sequence, uint32_t identity, uint8_t mode,
                        uint16_t target, const std::array<float, 3>& destination, uint32_t token, float speed,
                        Buffer& out) {
    if (!MissileBuild(build) || !agent || !identity || (identity & 0x80000000u) || mode > 3 ||
        !MissilePoint(destination) || !std::isfinite(speed) || speed <= 0)
        return false;
    std::array<uint8_t, 40> input{};
    const uint32_t target32 = target;
    std::memcpy(input.data() + 1, &identity, 4);
    input[5] = mode;
    std::memcpy(input.data() + 6, &target32, 4);
    std::memcpy(input.data() + 10, destination.data(), 12);
    std::memcpy(input.data() + 22, &token, 4);
    std::memcpy(input.data() + 26, &speed, 4);
    Buffer result;
    const uint8_t length = build == 14075 ? 31 : 40;
    if (!result.U16(build == 14075 ? 39 : 48) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(length) || !result.Append(ByteView(input.data(), length)))
        return false;
    out = std::move(result);
    return true;
}
inline bool BetaMissileHit(unsigned build, uint16_t agent, uint32_t sequence, uint32_t identity, uint16_t target,
                           Buffer& out) {
    // Own beta CMissile input tag1 requires exactly9 bytes and resolves a character, then notifies CAvMissile's
    // actor-hit observer. No damage.
    if (!MissileBuild(build) || !agent || !target || agent == target || !identity || (identity & 0x80000000u))
        return false;
    Buffer result;
    if (!result.U16(build == 14075 ? 39 : 48) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(9) || !result.U8(1) || !result.U32(identity) || !result.U32(target))
        return false;
    out = std::move(result);
    return true;
}
inline bool JuneMissileHit(unsigned build, uint16_t agent, uint32_t sequence, uint32_t identity, uint16_t target,
                           Buffer& out) {
    return (build == 14422 || build == 14505) && BetaMissileHit(build, agent, sequence, identity, target, out);
}
inline bool JuneMissileSurfaceImpact(unsigned build, uint16_t agent, uint32_t sequence, uint32_t identity,
                                     Buffer& out) {
    // Own input tag2 notifies the surface observer; the client checks geometry.
    if ((build != 14422 && build != 14505) || !agent || !identity || (identity & 0x80000000u))
        return false;
    Buffer result;
    if (!result.U16(48) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(5) || !result.U8(2) || !result.U32(identity))
        return false;
    out = std::move(result);
    return true;
}
inline bool DestroyMissile(unsigned build, uint16_t agent, uint32_t sequence, Buffer& out) {
    Buffer result;
    if (!MissileBuild(build) || !agent || !result.U16(build == 14075 ? 15 : 16) || !result.U16(agent) ||
        !result.VarU32(sequence) || !result.U8(0x80))
        return false;
    out = std::move(result);
    return true;
}
inline uint16_t MissileAgent(uint16_t owner) {
    return owner >= 257 && owner <= 510 ? static_cast<uint16_t>(owner + 768) : 0;
}
// Shared lifetime and late-observer position from the accepted July projectile.
struct MissileFlight {
    unsigned phase = 0;
    uint32_t identity = 0, skill = 0, token = 0;
    uint16_t agent = 0;
    uint8_t mode = 0;
    float speed = 0;
    uint64_t at = 0;
    std::array<float, 3> destination{}, start{};
    uint64_t launchedAt = 0;
    uint16_t Agent(uint16_t owner) const {
        return agent ? agent : MissileAgent(owner);
    }
    std::array<float, 3> PositionAt(uint64_t now) const {
        auto position = start;
        const double fraction = now <= launchedAt ? 0.0 : double(now - launchedAt) / double(at - launchedAt);
        for (size_t i = 0; i < 3; ++i)
            position[i] += static_cast<float>(fraction * (destination[i] - position[i]));
        return position;
    }
    bool Snapshot(unsigned build, uint16_t owner, uint64_t now, uint32_t sequence, Buffer& out) const {
        if (phase != 2 || now >= at || at <= launchedAt)
            return false;
        Buffer create, move;
        if (!CreateMissile(build, Agent(owner), sequence, identity, owner, skill, PositionAt(now), token, create) ||
            !MoveMissile(build, Agent(owner), sequence, identity, mode, 0, destination, token, speed, move) ||
            !create.Append(move.View()))
            return false;
        out = std::move(create);
        return true;
    }
    bool Advance(unsigned build, uint16_t owner, const std::array<float, 3>& position, uint64_t now, uint32_t sequence,
                 Buffer& out) {
        if (!phase || now < at)
            return true;
        uint64_t end = 0;
        if (phase == 1) {
            if (!MissilePoint(position))
                return false;
            const double dx = double(destination[0]) - position[0], dy = double(destination[1]) - position[1];
            const double dz = mode == 2 ? double(destination[2]) - position[2] : 0;
            const double distance = std::sqrt(dx * dx + dy * dy + dz * dz), seconds = distance / speed;
            if (!std::isfinite(seconds) || distance <= 1 || seconds <= 0 || seconds > 30) {
                phase = 0;
                return true;
            }
            const auto travel = static_cast<uint32_t>(std::ceil(seconds * 1000));
            if (now > UINT64_MAX - travel)
                return false;
            end = now + travel;
        }
        Buffer bytes, move;
        if (phase == 1) {
            if (!CreateMissile(build, Agent(owner), sequence, identity, owner, skill, position, token, bytes) ||
                !MoveMissile(build, Agent(owner), sequence, identity, mode, 0, destination, token, speed, move) ||
                !bytes.Append(move.View()))
                return false;
        } else if (!DestroyMissile(build, Agent(owner), sequence, bytes))
            return false;
        out = std::move(bytes);
        if (phase == 1) {
            phase = 2;
            at = end;
            start = position;
            launchedAt = now;
        } else
            phase = 0;
        return true;
    }
};
} // namespace beta
} // namespace glide
