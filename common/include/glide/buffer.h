#pragma once

#include <cstddef>
#include <cstdint>

namespace glide {

struct ByteView {
    const uint8_t* data;
    size_t size;

    ByteView() : data(nullptr), size(0) {}
    ByteView(const void* bytes, size_t length) : data(static_cast<const uint8_t*>(bytes)), size(length) {}
    bool Valid() const {
        return data != nullptr || size == 0;
    }
};

enum class DecodeStatus { Complete, Incomplete, Malformed, Unknown };

class Buffer {
  public:
    Buffer();
    ~Buffer();
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    const uint8_t* Data() const {
        return data_;
    }
    uint8_t* Data() {
        return data_;
    }
    size_t Size() const {
        return size_;
    }
    ByteView View() const {
        return ByteView(data_, size_);
    }
    bool Reserve(size_t capacity);
    bool Resize(size_t size); // Newly exposed bytes are zeroed.
    bool Append(ByteView bytes);
    bool U8(uint8_t value);
    bool U16(uint16_t value);
    bool U32(uint32_t value);
    bool U64(uint64_t value);
    bool F32(float value);
    bool VarU32(uint32_t value);
    bool Utf16Z(const char* utf8, size_t maxUnits);
    void Clear() {
        size_ = 0;
    }
    void Consume(size_t size);
    void Swap(Buffer& other) noexcept;

  private:
    uint8_t* data_;
    size_t size_;
    size_t capacity_;
};

class Reader {
  public:
    explicit Reader(ByteView bytes) : bytes_(bytes), position_(0) {}
    size_t Position() const {
        return position_;
    }
    size_t Remaining() const {
        return bytes_.size - position_;
    }
    bool Bytes(size_t count, ByteView& out);
    bool U8(uint8_t& out);
    bool U16(uint16_t& out);
    bool U32(uint32_t& out);
    DecodeStatus VarU32(uint32_t& out);

  private:
    ByteView bytes_;
    size_t position_;
};

} // namespace glide
