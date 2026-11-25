#pragma once
#include "glide/item_bindings.h"
namespace glide {
namespace hot49284 {
using StarterItemBinding = ItemBinding;
inline const StarterItemBinding* FindStarterItemBinding(uint32_t definition) {
    return FindItemBinding(49284, definition);
}
} // namespace hot49284
} // namespace glide
