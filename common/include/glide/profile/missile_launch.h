#pragma once
#include <array>

namespace glide {
// Existing modern visual policy: native Z points down. Offset both ends so
// weapon shots clear the ground without changing range, direction or actors.
inline void RaiseWeaponMissile(std::array<float, 3>& start, std::array<float, 3>& destination) {
    start[2] -= 39;
    destination[2] -= 39;
}
} // namespace glide
