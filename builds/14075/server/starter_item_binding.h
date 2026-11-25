#pragma once
#include "glide/item_bindings.h"
namespace glide {
namespace april14075 {
using StarterItemBinding = ItemBinding;
inline const StarterItemBinding* FindStarterItemBinding(uint32_t definition) {
    return FindItemBinding(14075, definition);
}
} // namespace april14075
} // namespace glide
