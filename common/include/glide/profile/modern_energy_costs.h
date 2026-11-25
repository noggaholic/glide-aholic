#pragma once

namespace glide {
namespace modern {
inline bool InitialEnergyCost(unsigned build, unsigned skill, unsigned& cost) {
    if (build != 167409 && build != 204489)
        return false;
    switch (skill) {
    case 26666:
        cost = 0;
        return true;
    case 26730:
        cost = 0;
        return true;
    case 27066:
        cost = 0;
        return true;
    case 27372:
        cost = 5;
        return true;
    case 27964:
        cost = 5;
        return true;
    case 28357:
        cost = 5;
        return true;
    case 28692:
        cost = 10;
        return true;
    case 28714:
        cost = 0;
        return true;
    case 28797:
        cost = 10;
        return true;
    case 28815:
        cost = 10;
        return true;
    case 28827:
        cost = 5;
        return true;
    case 28915:
        cost = 5;
        return true;
    case 28930:
        cost = 20;
        return true;
    default:
        return false;
    }
}
} // namespace modern
} // namespace glide
