#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
namespace glide {
// Read a field from a verified native runtime layout without alignment or aliasing assumptions.
// The caller remains responsible for validating the object and the build-specific offset.
template <typename T> T ReadRuntimeField(const void* object, size_t offset) {
    static_assert(std::is_trivially_copyable<T>::value, "Native fields must be trivially copyable");
    T value;
    std::memcpy(&value, static_cast<const uint8_t*>(object) + offset, sizeof(value));
    return value;
}
} // namespace glide
