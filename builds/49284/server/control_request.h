#pragma once
#include "glide/buffer.h"
namespace glide {
namespace hot49284 {
// Common native Create/World control messages. Atomic prefix parsing.
bool IsSharedControl(uint16_t id);
DecodeStatus DecodeControl(ByteView input, uint16_t& id, size_t& consumed);
} // namespace hot49284
} // namespace glide
