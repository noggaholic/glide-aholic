#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
// Structural validation only; does not establish authenticity or complete game content.
bool ValidateBxml(ByteView bytes, std::string& error);
bool DeriveContentKey(ByteView seed, uint8_t key[20]);
} // namespace glide
