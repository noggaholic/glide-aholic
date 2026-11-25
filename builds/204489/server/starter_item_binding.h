#pragma once
#include <cstdint>
namespace glide {
namespace build204489 {
// Generated from accepted original Build 204489 starter definitions and binding functions.
struct StarterItemBinding {
    uint32_t definition;
    uint8_t onUse, onAcquisition;
};
static const StarterItemBinding starter_item_bindings[] = {
    {6458, 2, 2},  {6459, 2, 2},  {6461, 2, 2},  {6464, 2, 2},  {6465, 2, 2},  {6467, 2, 2},  {6470, 2, 2},
    {6472, 2, 2},  {6473, 2, 2},  {6479, 2, 2},  {33343, 0, 0}, {33344, 0, 0}, {33345, 0, 0}, {33349, 0, 0},
    {33350, 0, 0}, {33355, 0, 0}, {33359, 0, 0}, {33360, 0, 0}, {33361, 0, 0},
};
inline const StarterItemBinding* FindStarterItemBinding(uint32_t definition) {
    for (const auto& rule : starter_item_bindings)
        if (rule.definition == definition)
            return &rule;
    return nullptr;
}
} // namespace build204489
} // namespace glide
