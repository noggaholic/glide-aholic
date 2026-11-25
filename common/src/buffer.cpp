#include "glide/buffer.h"

#include <Windows.h>
#include <cstdlib>
#include <cstring>
#include <utility>

namespace glide {

Buffer::Buffer() : data_(nullptr), size_(0), capacity_(0) {}
Buffer::~Buffer() {
    std::free(data_);
}
Buffer::Buffer(Buffer&& other) noexcept : Buffer() {
    Swap(other);
}
Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        Buffer old;
        Swap(old);
        Swap(other);
    }
    return *this;
}

void Buffer::Swap(Buffer& other) noexcept {
    std::swap(data_, other.data_);
    std::swap(size_, other.size_);
    std::swap(capacity_, other.capacity_);
}

bool Buffer::Reserve(size_t capacity) {
    if (capacity <= capacity_)
        return true;
    size_t next = capacity_ ? capacity_ : 256;
    while (next < capacity) {
        if (next > SIZE_MAX / 2) {
            next = capacity;
            break;
        }
        next *= 2;
    }
    void* replacement = std::realloc(data_, next);
    if (!replacement)
        return false;
    data_ = static_cast<uint8_t*>(replacement);
    capacity_ = next;
    return true;
}

bool Buffer::Resize(size_t size) {
    if (!Reserve(size))
        return false;
    if (size > size_)
        std::memset(data_ + size_, 0, size - size_);
    size_ = size;
    return true;
}

bool Buffer::Append(ByteView bytes) {
    if (!bytes.Valid() || bytes.size > SIZE_MAX - size_)
        return false;
    if (!bytes.size)
        return true;
    // Preserve a view into this buffer across a possible realloc.
    const uintptr_t source = reinterpret_cast<uintptr_t>(bytes.data);
    const uintptr_t begin = reinterpret_cast<uintptr_t>(data_);
    const bool internal = data_ && source >= begin && source - begin < capacity_;
    const size_t offset = internal ? static_cast<size_t>(source - begin) : 0;
    if (internal && (offset > size_ || bytes.size > size_ - offset))
        return false;
    if (!Reserve(size_ + bytes.size))
        return false;
    std::memmove(data_ + size_, internal ? data_ + offset : bytes.data, bytes.size);
    size_ += bytes.size;
    return true;
}

bool Buffer::U8(uint8_t value) {
    return Append(ByteView(&value, 1));
}
bool Buffer::U16(uint16_t value) {
    const uint8_t bytes[] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8)};
    return Append(ByteView(bytes, sizeof(bytes)));
}
bool Buffer::U32(uint32_t value) {
    const uint8_t bytes[] = {static_cast<uint8_t>(value), static_cast<uint8_t>(value >> 8),
                             static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 24)};
    return Append(ByteView(bytes, sizeof(bytes)));
}
bool Buffer::U64(uint64_t value) {
    uint8_t bytes[8];
    for (size_t i = 0; i < sizeof(bytes); ++i)
        bytes[i] = static_cast<uint8_t>(value >> (8 * i));
    return Append(ByteView(bytes, sizeof(bytes)));
}
bool Buffer::F32(float value) {
    uint32_t bits;
    static_assert(sizeof(bits) == sizeof(value), "32-bit float required");
    std::memcpy(&bits, &value, sizeof(bits));
    return U32(bits);
}
bool Buffer::VarU32(uint32_t value) {
    uint8_t bytes[5];
    size_t count = 0;
    do {
        bytes[count] = static_cast<uint8_t>(value & 127);
        value >>= 7;
        if (value)
            bytes[count] |= 128;
        ++count;
    } while (value);
    return Append(ByteView(bytes, count));
}
bool Buffer::Utf16Z(const char* utf8, size_t maxUnits) {
    if (!utf8)
        return false;
    const int units = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
    if (units <= 0 || static_cast<size_t>(units - 1) > maxUnits)
        return false;
    Buffer encoded;
    const size_t bytes = static_cast<size_t>(units) * sizeof(wchar_t);
    if (!encoded.Reserve(bytes))
        return false;
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, reinterpret_cast<wchar_t*>(encoded.Data()),
                            units) != units)
        return false;
    return Append(ByteView(encoded.Data(), bytes));
}

void Buffer::Consume(size_t count) {
    if (count >= size_) {
        Clear();
        return;
    }
    if (count)
        std::memmove(data_, data_ + count, size_ - count);
    size_ -= count;
}

bool Reader::Bytes(size_t count, ByteView& out) {
    if (!bytes_.Valid() || count > Remaining())
        return false;
    out = ByteView(bytes_.data ? bytes_.data + position_ : nullptr, count);
    position_ += count;
    return true;
}
bool Reader::U8(uint8_t& out) {
    ByteView part;
    if (!Bytes(1, part))
        return false;
    out = part.data[0];
    return true;
}
bool Reader::U16(uint16_t& out) {
    ByteView part;
    if (!Bytes(2, part))
        return false;
    out = static_cast<uint16_t>(part.data[0] | (static_cast<uint16_t>(part.data[1]) << 8));
    return true;
}
bool Reader::U32(uint32_t& out) {
    ByteView part;
    if (!Bytes(4, part))
        return false;
    out = 0;
    for (unsigned i = 0; i < 4; ++i)
        out |= static_cast<uint32_t>(part.data[i]) << (i * 8);
    return true;
}
DecodeStatus Reader::VarU32(uint32_t& out) {
    if (!bytes_.Valid())
        return DecodeStatus::Malformed;
    uint32_t result = 0;
    for (size_t i = 0; i < 5; ++i) {
        if (i >= Remaining())
            return DecodeStatus::Incomplete;
        const uint8_t byte = bytes_.data[position_ + i];
        if (i == 4 && (byte & 0xf0))
            return DecodeStatus::Malformed;
        result |= static_cast<uint32_t>(byte & 127) << (7 * i);
        if (!(byte & 128)) {
            position_ += i + 1;
            out = result;
            return DecodeStatus::Complete;
        }
    }
    return DecodeStatus::Malformed;
}

} // namespace glide
