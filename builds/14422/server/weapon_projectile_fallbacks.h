#pragma once

#include "weapon_projectiles.h"
namespace glide {
namespace june14422 {
inline const WeaponProjectile* FindWeaponProjectileRule(unsigned skill) {
    if (const auto* own = FindWeaponProjectile(skill))
        return own;
    static const WeaponProjectile fallbacks[] = {
        {4539, 700, 600.0f},  {4165, 1500, 600.0f}, {4710, 800, 600.0f},  {4166, 1400, 600.0f},
        {2674, 1099, 600.0f}, {2676, 1667, 600.0f}, {3218, 800, 600.0f},  {3221, 800, 600.0f},
        {3220, 1500, 600.0f}, {437, 1000, 600.0f},  {2907, 1000, 600.0f}, {2960, 1000, 600.0f},
        {2961, 700, 600.0f},  {4310, 700, 600.0f},  {4330, 700, 600.0f},  {6225, 700, 600.0f},
    };
    for (const auto& rule : fallbacks)
        if (rule.skill == skill)
            return &rule;
    return nullptr;
}
} // namespace june14422
} // namespace glide
