#pragma once
#include "glide/buffer.h"

namespace glide {
// Maximum bounds the declared full size. Prefix optionally reads only the beginning.
// Output is replaced only on success. Input contains complete little-endian words.
bool InflateArchive(ByteView input, Buffer& output, size_t maximum = 512u * 1024u * 1024u, size_t prefix = 0);
} // namespace glide
