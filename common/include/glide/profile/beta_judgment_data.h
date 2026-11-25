#pragma once

namespace glide {
namespace beta {
struct Judgment4522 {
    enum : unsigned { Skill = 4522, ContactRadius = 32, BatchLimit = 5 };
    static float Amount(unsigned level) {
        return level && level <= 80 ? 5.0f + 0.01f * level * level : 0.0f;
    }
};
} // namespace beta
} // namespace glide
