#pragma once

namespace glide {
namespace hot49284 {
struct EngineerTerminalRule {
    unsigned skill, appearanceSkill, lifetimeMs;
};
static const EngineerTerminalRule kEngineerTerminalRules[] = {{5941, 6073, 1000}, {6147, 6073, 1000}};
inline const EngineerTerminalRule* EngineerTerminalForSkill(unsigned skill) {
    for (const auto& rule : kEngineerTerminalRules)
        if (rule.skill == skill)
            return &rule;
    return nullptr;
}
} // namespace hot49284
} // namespace glide
