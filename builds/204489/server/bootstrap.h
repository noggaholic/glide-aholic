#pragma once
#include "glide/profile/game_bootstrap.h"
#include <memory>

namespace glide {
// Verified shared packet layouts, with explicit regional differences.
struct ModernWorldDialect {
    uint32_t build = 204489;
    uint16_t (*messageId)(uint16_t) = nullptr;
    uint16_t affinityId = 0x0097;
    bool compactAffinity = false;
    bool structuralBags = true;
    bool wallet = true;
    uint32_t mapReadyTimeoutMs = 120000;
    uint16_t accountConfigurationWords = 10;
    uint16_t Id(uint16_t canonical) const {
        return messageId ? messageId(canonical) : canonical;
    }
};
std::unique_ptr<GameBootstrap> CreateModernWorldBootstrap(const ModernWorldDialect& dialect);
} // namespace glide
