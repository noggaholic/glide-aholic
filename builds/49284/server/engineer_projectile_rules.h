#pragma once

namespace glide {
namespace hot49284 {
struct EngineerProjectileRule {
    unsigned skill, followup, marker, slot, launchMs, range, count, holdMs;
    float spreadRadians;
};
static const EngineerProjectileRule kEngineerProjectileRules[] = {{5941, 6073, 6075, 6, 280, 1000, 3, 6000, 0.15f},
                                                                  {6147, 6073, 6075, 6, 280, 1200, 3, 6000, 0.15f},
                                                                  {5879, 6074, 6076, 9, 280, 1000, 1, 0, 0.00f},
                                                                  {6145, 6074, 6076, 9, 280, 1200, 1, 0, 0.00f}};
inline const EngineerProjectileRule* EngineerProjectileForSkill(unsigned skill) {
    for (const auto& r : kEngineerProjectileRules)
        if (r.skill == skill)
            return &r;
    return nullptr;
}
} // namespace hot49284
} // namespace glide
