#pragma once

#include "glide/profile/beta_condition_data.h"
namespace glide {
namespace modern {
constexpr unsigned kFirstConditionInstance = 0xffff0000u;
constexpr unsigned kDwarfConditionRemovalLimit = 5;
inline bool IsConditionDefinition(unsigned build, unsigned buff) {
    if (beta::IsOwnCondition(build, buff))
        return true;
    if (build == 167409)
        switch (buff) {
        case 720:
        case 721:
        case 722:
        case 727:
        case 736:
        case 737:
        case 738:
        case 742:
        case 21632:
        case 26766:
        case 27705:
        case 36815:
            return true;
        default:
            return false;
        }
    if (build == 204489)
        switch (buff) {
        case 720:
        case 721:
        case 722:
        case 727:
        case 736:
        case 737:
        case 738:
        case 742:
        case 861:
        case 21632:
        case 26766:
        case 27705:
        case 36815:
            return true;
        default:
            return false;
        }
    if (build == 49284)
        switch (buff) {
        case 720:
        case 721:
        case 722:
        case 727:
        case 736:
        case 737:
        case 738:
        case 742:
        case 861:
        case 19426:
        case 21632:
        case 26766:
            return true;
        default:
            return false;
        }
    return false;
}
} // namespace modern
} // namespace glide
