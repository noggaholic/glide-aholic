#pragma once
#include <cstdint>
namespace glide {
namespace june14505 {
// Own original ItemDef rules for both June builds; absent definitions fail lookup.
struct StarterItemBinding {
    unsigned build;
    uint32_t definition;
    bool onUse;
};
static const StarterItemBinding starter_item_bindings[] = {
    {14422, 4221, false},  {14422, 4223, false},  {14422, 4227, false},  {14422, 4228, false},  {14422, 4230, false},
    {14422, 4233, false},  {14422, 4236, true},   {14422, 4237, true},   {14422, 4238, true},   {14422, 4239, true},
    {14422, 4240, true},   {14422, 4241, true},   {14422, 4242, true},   {14422, 4243, true},   {14422, 4244, true},
    {14422, 22203, false}, {14422, 22204, false}, {14422, 22205, false}, {14505, 4221, false},  {14505, 4223, false},
    {14505, 4227, false},  {14505, 4228, false},  {14505, 4230, false},  {14505, 4233, false},  {14505, 4236, true},
    {14505, 4237, true},   {14505, 4238, true},   {14505, 4239, true},   {14505, 4240, true},   {14505, 4241, true},
    {14505, 4242, true},   {14505, 4243, true},   {14505, 4244, true},   {14505, 22203, false}, {14505, 22204, false},
    {14505, 22205, false}};
inline const StarterItemBinding* FindStarterItemBinding(unsigned build, uint32_t definition) {
    for (const auto& value : starter_item_bindings)
        if (value.build == build && value.definition == definition)
            return &value;
    return nullptr;
}
} // namespace june14505
} // namespace glide
