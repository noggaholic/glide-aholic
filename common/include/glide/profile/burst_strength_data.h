#pragma once

namespace glide {
namespace modern {
// Both records have null trait/predicate links. Select this personal component explicitly; do not widen the
// general fact filter or simulate its damage bonus.
struct BurstStrength {
    enum : unsigned { Skill = 28113, Base = 26644, Buff = 51653, Duration = 10000 };
};
} // namespace modern
} // namespace glide
