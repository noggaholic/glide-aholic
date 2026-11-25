#pragma once
#include <cstdint>
namespace glide {
namespace china167409 {
// Original China starter definitions and ItemConst use/acquisition rules.
struct StarterItemBinding {
    uint32_t definition;
    uint8_t onUse, onAcquisition;
};
static const StarterItemBinding starter_item_bindings[] = {
    {6458, 2, 2},  {6459, 2, 2},  {6460, 2, 2},  {6461, 2, 2},  {6462, 2, 2},  {6463, 2, 2},  {6464, 2, 2},
    {6465, 2, 2},  {6466, 2, 2},  {6467, 2, 2},  {6468, 2, 2},  {6469, 2, 2},  {6470, 2, 2},  {6471, 2, 2},
    {6472, 2, 2},  {6473, 2, 2},  {6474, 2, 2},  {6475, 2, 2},  {6476, 2, 2},  {6479, 2, 2},  {6541, 2, 2},
    {6542, 2, 2},  {6543, 2, 2},  {6544, 2, 2},  {6545, 2, 2},  {6546, 2, 2},  {6548, 2, 2},  {6549, 2, 2},
    {6550, 2, 2},  {6551, 2, 2},  {6552, 2, 2},  {6553, 2, 2},  {33343, 0, 0}, {33344, 0, 0}, {33345, 0, 0},
    {33349, 0, 0}, {33350, 0, 0}, {33355, 0, 0}, {33359, 0, 0}, {33360, 0, 0}, {33361, 0, 0}, {69145, 2, 2},
    {69244, 2, 2}, {69420, 2, 2}};
inline const StarterItemBinding* FindStarterItemBinding(uint32_t definition) {
    for (const auto& value : starter_item_bindings)
        if (value.definition == definition)
            return &value;
    return nullptr;
}
} // namespace china167409
} // namespace glide
