#pragma once
// Directed single-contact policy only.
namespace glide {
namespace combat {
inline bool BasicContactSkill(unsigned build, unsigned skill) {
    switch (build) {
    case 49284:
        switch (skill) {
        case 5489:
        case 5598:
        case 5604:
        case 5656:
        case 5657:
        case 5827:
        case 5877:
        case 6042:
        case 6148:
        case 9108:
        case 9109:
        case 9110:
        case 9189:
        case 10170:
        case 10171:
        case 10172:
        case 10258:
        case 10355:
        case 10561:
        case 10617:
        case 10692:
        case 10693:
        case 12526:
        case 12607:
        case 12642:
        case 13004:
        case 13005:
        case 13087:
        case 13108:
        case 13119:
        case 13120:
        case 13121:
        case 13125:
        case 14363:
        case 14364:
        case 14365:
        case 14437:
        case 14438:
        case 14439:
        case 15716:
        case 15717:
        case 15718:
        case 26666:
        case 26730:
        case 27066:
        case 28714:
            return true;
        default:
            return false;
        }
    case 167409:
        switch (skill) {
        case 5489:
        case 5598:
        case 5604:
        case 5656:
        case 5657:
        case 5827:
        case 6148:
        case 9108:
        case 9109:
        case 9110:
        case 9189:
        case 10170:
        case 10171:
        case 10172:
        case 10258:
        case 10561:
        case 10617:
        case 10692:
        case 10693:
        case 12526:
        case 13004:
        case 13087:
        case 13108:
        case 13119:
        case 13120:
        case 13121:
        case 13125:
        case 14363:
        case 14364:
        case 14365:
        case 14437:
        case 14438:
        case 14439:
        case 15716:
        case 15717:
        case 15718:
        case 26666:
        case 26730:
        case 27066:
        case 28714:
            return true;
        default:
            return false;
        }
    case 204489:
        switch (skill) {
        case 5489:
        case 5598:
        case 5604:
        case 5656:
        case 5657:
        case 5827:
        case 6148:
        case 9108:
        case 9109:
        case 9110:
        case 9189:
        case 10170:
        case 10171:
        case 10172:
        case 10258:
        case 10561:
        case 10617:
        case 10692:
        case 10693:
        case 12526:
        case 13004:
        case 13087:
        case 13108:
        case 13119:
        case 13120:
        case 13121:
        case 13125:
        case 14363:
        case 14364:
        case 14365:
        case 14437:
        case 14438:
        case 14439:
        case 15716:
        case 15717:
        case 15718:
        case 26666:
        case 26730:
        case 27066:
        case 28714:
            return true;
        default:
            return false;
        }
    default:
        return false;
    }
}
// Retain the already documented order of the eight initial melee families.
inline unsigned BasicChainNext(unsigned build, unsigned previous) {
    if (build != 167409 && build != 204489)
        return 0;
    switch (previous) {
    case 9109:
        return 9110;
    case 9110:
        return 9108;
    case 9108:
        return 9109;
    case 14364:
        return 14365;
    case 14365:
        return 14363;
    case 14363:
        return 14364;
    case 14437:
        return 14438;
    case 14438:
        return 14439;
    case 14439:
        return 14437;
    case 13004:
        return 13087;
    case 13087:
        return 13108;
    case 13108:
        return 13004;
    case 13119:
        return 13120;
    case 13120:
        return 13121;
    case 13121:
        return 13119;
    case 10170:
        return 10171;
    case 10171:
        return 10172;
    case 10172:
        return 10170;
    case 10692:
        return 10693;
    case 10693:
        return 10617;
    case 10617:
        return 10692;
    case 27066:
        return 26730;
    case 26730:
        return 26666;
    case 26666:
        return 27066;
    default:
        return 0;
    }
}
inline bool BasicChainContinuation(unsigned skill) {
    switch (skill) {
    case 9108:
    case 9110:
    case 10171:
    case 10172:
    case 10617:
    case 10693:
    case 13087:
    case 13108:
    case 13120:
    case 13121:
    case 14363:
    case 14365:
    case 14438:
    case 14439:
    case 26666:
    case 26730:
        return true;
    default:
        return false;
    }
}
// Existing base-elemental selection; each ID is present in both modern catalogs.
inline unsigned BasicAttunement(unsigned skill) {
    switch (skill) {
    case 5489:
        return 3;
    case 5598:
        return 1;
    case 5604:
        return 2;
    case 5656:
        return 3;
    case 5657:
        return 4;
    case 15716:
        return 2;
    case 15717:
        return 4;
    case 15718:
        return 1;
    default:
        return 0;
    }
}
} // namespace combat
} // namespace glide
