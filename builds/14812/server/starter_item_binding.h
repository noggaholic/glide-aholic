#pragma once
#include <cstdint>
namespace glide {
namespace july14812 {
// Original July starter ItemDef flags and RequiresSoulbindBeforeUse rule.
struct StarterItemBinding {
    uint32_t definition;
    uint8_t onUse, onAcquisition;
};
static const StarterItemBinding starter_item_bindings[] = {
    {4221, 0, 0}, {4223, 0, 0}, {4227, 0, 0}, {4228, 0, 0},  {4230, 0, 0},  {4233, 0, 0},
    {4236, 2, 2}, {4237, 2, 2}, {4238, 2, 2}, {4239, 2, 2},  {4240, 2, 2},  {4241, 2, 2},
    {4242, 2, 2}, {4243, 2, 2}, {4244, 2, 2}, {22203, 0, 0}, {22204, 0, 0}, {22205, 0, 0}};
inline const StarterItemBinding* FindStarterItemBinding(uint32_t definition) {
    for (const auto& value : starter_item_bindings)
        if (value.definition == definition)
            return &value;
    return nullptr;
}
} // namespace july14812
} // namespace glide
