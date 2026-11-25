#pragma once

namespace glide {
namespace china167409 {
struct DwarfHealingData {
    enum : unsigned {
        Skill = 27372,
        Legend = 16,
        Slot = 0,
        MinimumLevel = 1,
        ActivationMs = 1333,
        AnimationMs = 1853,
        RechargeMs = 30000
    };
    static int Amount(unsigned level) {
        if (!level || level > 80)
            return 0;
        return 125 + static_cast<int>(static_cast<float>(level * level) * 0.84f + 0.5f);
    }
};
} // namespace china167409
} // namespace glide
