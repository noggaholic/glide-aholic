#pragma once
// Window follows own base phases; adapted duration.
namespace glide {
namespace china167409 {
struct StanceRule {
    unsigned profession, skill, followup, buff, slot, durationMs;
};
static const StanceRule kAquaticStance = {5, 13122, 13130, 13124, 7, 2033};
inline const StanceRule* StanceForSkill(unsigned skill) {
    return skill == kAquaticStance.skill ? &kAquaticStance : nullptr;
}
inline const StanceRule* StanceForFollowup(unsigned skill) {
    return skill == kAquaticStance.followup ? &kAquaticStance : nullptr;
}
} // namespace china167409
} // namespace glide
