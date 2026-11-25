#pragma once
#include <cstdint>

namespace glide {
namespace modern {
// The modern default selects the authored mode4/flags2 durations for 740/717. It does not enable flags12 or claim
// an inferred PvP/WvW mode mapping.
template <unsigned Build> unsigned MaximumReceivedEffectDuration(unsigned definition) {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported allied-effect dialect");
    switch (definition) {
    case 1122:
        return Build == 49284 ? 6000 : 3000;
    case 718:
        return Build == 49284 ? 3000 : 4000;
    case 725:
        return Build == 49284 ? 1500 : 3000;
    case 740:
        return Build == 49284 ? 5000 : 12000;
    case 719:
        return 3000;
    case 873:
        return Build == 49284 ? 1000 : 0;
    case 717:
        return Build == 49284 ? 0 : 3000;
    case 5974:
        return Build == 49284 ? 3000 : 5000;
    case 26596:
        return 5000;
    default:
        return 0;
    }
}
template <unsigned Build> bool AllowedReceivedEffect(unsigned definition, uint64_t remainingMs) {
    const auto maximum = MaximumReceivedEffectDuration<Build>(definition);
    return remainingMs && maximum && remainingMs <= maximum;
}
inline bool AllowedReceivedEffect(unsigned build, unsigned definition, uint64_t remainingMs) {
    // July4606 also delivers its own allied boons on contact.
    if (build == 14812 && remainingMs &&
        ((definition == 347 && remainingMs <= 5000) ||
         ((definition == 290 || definition == 951) && remainingMs <= 2000)))
        return true;
    if (build == 14075 || build == 14422 || build == 14505 || build == 14812)
        return remainingMs &&
               ((definition == 289 && remainingMs <= 1000) || (definition == 280 && remainingMs <= 3000));
    switch (build) {
    case 49284:
        return AllowedReceivedEffect<49284>(definition, remainingMs);
    case 167409:
        return AllowedReceivedEffect<167409>(definition, remainingMs);
    case 204489:
        return AllowedReceivedEffect<204489>(definition, remainingMs);
    default:
        return false;
    }
}
} // namespace modern
} // namespace glide
