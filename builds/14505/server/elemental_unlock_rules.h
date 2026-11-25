#pragma once
#include <cstdint>

namespace glide {
namespace june14505 {
struct ElementalUnlockRule {
    uint32_t trait;
    unsigned minimumLevel;
};
constexpr ElementalUnlockRule kOwnJuneElementalUnlocks[] = {
    {243, 1},
    {240, 1},
    {241, 3},
    {242, 6},
};
} // namespace june14505
} // namespace glide
