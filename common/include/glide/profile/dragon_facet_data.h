#pragma once
#include "glide/profile/world_actions.h"
#include <vector>

namespace glide {
namespace modern {

struct FacetRule {
    unsigned base = 0, alternate = 0, slot = 0, legend = 0, marker = 0, boon = 0, duration = 0;
    unsigned activation = 0, animation = 0, recharge = 0;
    int upkeep = 0;
};
template <unsigned Build> struct FacetData {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported facet dialect");
    enum : unsigned {
        Legend = Build == 49284    ? 11
                 : Build == 167409 ? 14
                                   : 15,
        Pulse = 3000,
        Radius = 600,
        MaximumAllies = 4
    };
    static unsigned BaseFor(unsigned skill) {
        switch (skill) {
        case 27220:
        case 27228:
            return 27220;
        case 28379:
        case 27080:
            return 28379;
        case 26644:
        case 28113:
            return 26644;
        case 27014:
        case 27162:
            return 27014;
        case 27760:
        case 28075:
            return 27760;
        default:
            return 0;
        }
    }
    static bool IsFacetSkill(unsigned skill) {
        return BaseFor(skill) != 0;
    }
    static bool IsBase(unsigned skill) {
        return skill && skill == BaseFor(skill);
    }
    static FacetRule RuleFor(unsigned skill, bool water = false) {
        FacetRule r;
        r.base = BaseFor(skill);
        if (!r.base)
            return r;
        r.legend = Legend;
        r.activation = 700;
        r.animation = 1500;
        switch (r.base) {
        case 27220:
            r.alternate = 27228;
            r.slot = 0;
            r.marker = 27336;
            r.boon = 718;
            r.duration = Build == 49284 ? 3000 : 4000;
            r.upkeep = -1;
            r.activation = Build == 49284 ? 700 : water ? 320 : 520;
            r.animation = Build == 49284 ? 1500 : water ? 1083 : 1300;
            break;
        case 28379:
            r.alternate = 27080;
            r.slot = 1;
            r.marker = 28036;
            r.boon = 725;
            r.duration = Build == 49284 ? 1500 : 3000;
            r.upkeep = -2;
            break;
        case 26644:
            r.alternate = 28113;
            r.slot = Build == 49284 ? 2 : 3;
            r.marker = 27376;
            r.boon = 740;
            r.duration = Build == 49284 ? 5000 : 12000;
            r.upkeep = Build == 49284 ? -1 : -2;
            break;
        case 27014:
            r.alternate = 27162;
            r.slot = Build == 49284 ? 3 : 2;
            r.marker = 28243;
            r.boon = 719;
            r.duration = 3000;
            r.upkeep = -1;
            break;
        case 27760:
            r.alternate = 28075;
            r.slot = 4;
            r.marker = 27983;
            r.boon = Build == 49284 ? 873 : 717;
            r.duration = Build == 49284 ? 1000 : 3000;
            r.upkeep = Build == 49284 ? -3 : -4;
            break;
        }
        if (skill == r.alternate) {
            r.activation = r.animation = 0;
            switch (skill) {
            case 27228:
                r.recharge = 30000;
                break;
            case 27080:
                r.recharge = Build == 49284 ? 20000 : 15000;
                break;
            case 28113:
                r.activation = Build == 49284 || !water ? 1200 : 1150;
                r.animation = Build == 49284 || !water ? 1800 : 2367;
                r.recharge = Build == 49284 ? 15000 : 12000;
                break;
            case 27162:
                r.activation = Build == 49284 || !water ? 700 : 550;
                r.animation = Build == 49284 || !water ? 1500 : 1166;
                r.recharge = Build == 49284 ? 15000 : 12000;
                break;
            case 28075:
                r.activation = Build == 49284 || water ? 1300 : 866;
                r.animation = Build == 49284 ? 1500 : water ? 3000 : 1000;
                r.recharge = Build == 49284 ? 45000 : 20000;
                break;
            }
        }
        return r;
    }
};
enum class FacetPulseStatus { None, Ready, Invalid };
struct FacetPulseSelection {
    unsigned base = 0;
    uint64_t nominal = 0;
    int ownIndex = -1;
    std::vector<unsigned> allyIndices;
};
struct FacetPulsePlan {
    uint64_t revision = 0, utc = 0;
    std::vector<ReceivedWorldEffect> own;
    std::vector<WorldReceivedEffectTarget> allies;
    std::vector<FacetPulseSelection> selected;
};
} // namespace modern
} // namespace glide
