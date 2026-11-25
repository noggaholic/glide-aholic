#pragma once

namespace glide {
namespace june14505 {
struct WeaponProjectile {
    unsigned skill, launchMs;
    float speed;
};
inline const WeaponProjectile* FindWeaponProjectile(unsigned skill) {
    static const WeaponProjectile rules[] = {
        {4522, 1000, 600.0f}, {2782, 760, 600.0f}, {1487, 1000, 905.0f}, {4606, 1000, 600.0f}, {1006, 1050, 905.0f},
    };
    for (const auto& rule : rules)
        if (rule.skill == skill)
            return &rule;
    return nullptr;
}
} // namespace june14505
} // namespace glide
