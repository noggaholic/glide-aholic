#pragma once

namespace glide {
namespace beta {
struct Siren4606 {
    enum : unsigned { Skill = 4606, ContactRadius = 32, Might = 347, Swiftness = 290, Vigor = 951 };
    // Stable selection is a compatible policy, not the historical random script.
    // Retrying a failed save cannot select another boon.
    static unsigned Definition(unsigned identity) {
        return identity ? (identity - 1) % 3 == 0 ? Might : (identity - 1) % 3 == 1 ? Swiftness : Vigor : 0;
    }
    static unsigned Duration(unsigned definition) {
        return definition == Might ? 5000 : definition == Swiftness || definition == Vigor ? 2000 : 0;
    }
};
// Compatible first-contact healing, explicitly chosen for the older builds.
// Heal Amount=3 is known; the historical recipient/event script is not.
inline bool SirenHealingBuild(unsigned build) {
    return build == 14075 || build == 14422 || build == 14505;
}
inline float SirenHealingAmount() {
    return 3.0f;
}
} // namespace beta
} // namespace glide
