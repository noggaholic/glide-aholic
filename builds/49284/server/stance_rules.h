#pragma once

namespace glide {
namespace hot49284 {
struct StanceRule {
    unsigned profession, skill, followup, buff, slot, durationMs, professionState;
};
static const StanceRule kStanceRules[] = {
    {2, 14441, 14555, 14461, 8, 2033, 17}, {5, 13122, 13130, 13124, 7, 2033, 17}, {6, 5606, 5614, 5613, 7, 1667, 2}};
inline const StanceRule* StanceForSkill(unsigned skill) {
    for (const auto& r : kStanceRules)
        if (r.skill == skill)
            return &r;
    return nullptr;
}
inline const StanceRule* StanceForFollowup(unsigned skill) {
    for (const auto& r : kStanceRules)
        if (r.followup == skill)
            return &r;
    return nullptr;
}
} // namespace hot49284
} // namespace glide
