#include "glide/guid.h"
#include <cstring>

namespace glide {
namespace {
int Hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}
} // namespace
bool Guid::Parse(const std::string& text, Guid& out) {
    if (text.size() != 32 && text.size() != 36)
        return false;
    Guid result;
    size_t at = 0;
    for (size_t i = 0; i < 16; ++i) {
        if (text.size() == 36 && (i == 4 || i == 6 || i == 8 || i == 10)) {
            if (text[at++] != '-')
                return false;
        }
        const int high = Hex(text[at++]), low = Hex(text[at++]);
        if (high < 0 || low < 0)
            return false;
        result.bytes[i] = static_cast<uint8_t>((high << 4) | low);
    }
    out = result;
    return true;
}
std::string Guid::Text(bool separators) const {
    const char* hex = "0123456789abcdef";
    std::string text;
    for (size_t i = 0; i < 16; ++i) {
        if (separators && (i == 4 || i == 6 || i == 8 || i == 10))
            text += '-';
        text += hex[bytes[i] >> 4];
        text += hex[bytes[i] & 15];
    }
    return text;
}
bool Guid::Matches(ByteView observed) const {
    if (!observed.Valid() || observed.size != 16)
        return false;
    if (std::memcmp(bytes, observed.data, 16) == 0)
        return true;
    const unsigned order[] = {3, 2, 1, 0, 5, 4, 7, 6, 8, 9, 10, 11, 12, 13, 14, 15};
    for (size_t i = 0; i < 16; ++i)
        if (observed.data[i] != bytes[order[i]])
            return false;
    return true;
}
} // namespace glide
