#pragma once
#include "glide/profile/beta_missile.h"
#include "glide/profile/beta_rock_barrier_data.h"
#include "glide/profile/world_actions.h"
namespace glide {
namespace beta {
// Prepare on copies.0 declines a stale/range/slot request,1 is ready,-1 invalid.
template <class Shots, class Effects>
int PrepareRockThrow(uint16_t owner, uint32_t identity, uint32_t target, const WorldActionContext& c, Shots& shots,
                     Effects& effects, Buffer& bytes) {
    if (!effects.RockCount(c.now) || target == owner || target > UINT16_MAX)
        return 0;
    const std::array<float, 3> facing =
        c.hasFacing ? std::array<float, 3>{{c.facing.x, c.facing.y, c.facing.z}} : std::array<float, 3>{{1, 0, 0}};
    const double length = std::hypot(std::hypot(double(facing[0]), double(facing[1])), double(facing[2]));
    if (!std::isfinite(length) || length <= 0 || !MissileAgent(owner) || !identity || !c.wallNow)
        return -1;
    if (target) {
        CatalogPosition point;
        if (!c.isVisibleCharacterTarget || !c.isVisibleCharacterTarget(target) || !c.characterTargetPosition ||
            !c.characterTargetPosition(target, point))
            return 0;
        const double x = double(point.x) - c.position.x, y = double(point.y) - c.position.y,
                     z = double(point.z) - c.position.z;
        const double distance = std::sqrt(x * x + y * y + z * z) * 32;
        if (!std::isfinite(distance) || distance > RockBarrier6224::Range)
            return 0;
    }
    size_t index = 0;
    while (index < shots.size() && shots[index].flight.phase)
        ++index;
    if (index == shots.size())
        return 0;
    auto& shot = shots[index];
    shot = {};
    shot.target = target;
    shot.range = RockBarrier6224::Range;
    shot.facing = facing;
    auto& f = shot.flight;
    f.phase = 1;
    f.identity = identity;
    f.skill = RockBarrier6224::Hurl;
    f.agent = static_cast<uint16_t>(MissileAgent(owner) + 256 * (index + 1));
    f.mode = 2;
    f.speed = RockBarrier6224::Speed;
    f.at = c.now;
    return effects.ConsumeRock(owner, c.now, bytes) ? 1 : -1;
}
} // namespace beta
} // namespace glide
