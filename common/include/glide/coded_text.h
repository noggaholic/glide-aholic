#pragma once
#include "glide/buffer.h"
#include <string>
namespace glide {
// Retained original literal format. Capacity includes prefix and terminators.
inline bool AppendCodedText(const std::wstring& text, unsigned capacity, Buffer& out) {
    size_t units = 5;
    for (auto unit : text) {
        if (unit == 1 || unit == 2 || unit == 3)
            return false;
        units += unit == L'[' || unit == L']' || unit == L'\\' ? 2 : 1;
    }
    if (units > capacity || !out.U16(0x8101) || !out.U16(0x447b) || !out.U16(0x0107))
        return false;
    for (auto unit : text) {
        if ((unit == L'[' || unit == L']' || unit == L'\\') && !out.U16('\\'))
            return false;
        if (!out.U16(static_cast<uint16_t>(unit)))
            return false;
    }
    return out.U16(1) && out.U16(0);
}
} // namespace glide
