#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
struct Guid {
    uint8_t bytes[16] = {};
    static bool Parse(const std::string& text, Guid& out);
    std::string Text(bool separators = true) const;
    bool Matches(ByteView observed) const;
};
} // namespace glide
