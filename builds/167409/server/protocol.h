#pragma once
#include "glide/profile/wire_map.h"

namespace glide {
const WireMap& China167409Wire();
namespace china167409 {
// The retained table covers identical payload shapes only. Affinity uses its own layout.
uint16_t ServerMessageId(uint16_t canonical);
bool RemapServerMessage(Buffer& payload, size_t offset = 0);
} // namespace china167409
} // namespace glide
