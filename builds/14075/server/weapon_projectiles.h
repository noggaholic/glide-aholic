#pragma once

namespace glide {
namespace april14075 {
struct WeaponProjectile {
    unsigned skill, launchMs;
    float speed;
};
inline const WeaponProjectile* FindWeaponProjectile(unsigned skill) {
    static const WeaponProjectile rules[] = {
        {4522, 800, 600.0f}, {2782, 760, 600.0f}, {1487, 1000, 900.0f}, {4606, 800, 600.0f}, {1006, 1050, 900.0f},
    };
    for (const auto& rule : rules)
        if (rule.skill == skill)
            return &rule;
    return nullptr;
}
} // namespace april14075
} // namespace glide
