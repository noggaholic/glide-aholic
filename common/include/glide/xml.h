#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
bool XmlValue(ByteView xml, const char* tag, size_t limit, std::string& out);
std::string XmlEscape(const std::string& text);
} // namespace glide
