#pragma once
#include "glide/buffer.h"
#include "glide/files.h"
#include <array>
#include <cstring>
namespace glide {
namespace july14812 {
namespace detail {
template <size_t N> bool Read(Reader& reader, std::array<uint8_t, N>& out) {
    ByteView value;
    if (!reader.Bytes(N, value))
        return false;
    std::memcpy(out.data(), value.data, N);
    return true;
}
template <size_t N> bool Write(Buffer& out, const std::array<uint8_t, N>& value) {
    return out.Append(ByteView(value.data(), N));
}
inline bool Var(Reader& reader, uint32_t& out) {
    return reader.VarU32(out) == DecodeStatus::Complete;
}
inline DecodeStatus Text(Reader& reader, size_t capacity, std::string& out) {
    std::wstring value;
    for (size_t i = 0; i < capacity; ++i) {
        uint16_t unit;
        if (!reader.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit)
            return WideToUtf8(value, out) ? DecodeStatus::Complete : DecodeStatus::Malformed;
        value.push_back(static_cast<wchar_t>(unit));
    }
    return DecodeStatus::Malformed;
}
} // namespace detail
} // namespace july14812
} // namespace glide
