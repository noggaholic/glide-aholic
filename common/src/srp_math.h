#pragma once
#include "glide/srp.h"
#include <Windows.h>
#include <utility>

// Internal arithmetic shared by the server and the test-only SRP client.
namespace glide {
namespace srp_detail {
struct Number {
    uint32_t word[33] = {}; // Extra limb for addition/reduction carry.
    ~Number() {
        SecureZeroMemory(word, sizeof(word));
    }
};
struct Scratch {
    Buffer bytes;
    ~Scratch() {
        if (bytes.Size())
            SecureZeroMemory(bytes.Data(), bytes.Size());
    }
};
struct Digest {
    uint8_t bytes[20] = {};
    ~Digest() {
        SecureZeroMemory(bytes, sizeof(bytes));
    }
};
inline Number Read(ByteView bytes) {
    Number result;
    for (size_t i = 0; i < bytes.size; ++i)
        result.word[i / 4] |= static_cast<uint32_t>(bytes.data[bytes.size - 1 - i]) << ((i % 4) * 8);
    return result;
}
inline void Write(const Number& value, uint8_t bytes[128]) {
    for (size_t i = 0; i < 128; ++i)
        bytes[127 - i] = static_cast<uint8_t>(value.word[i / 4] >> ((i % 4) * 8));
}
inline const Number& Prime() {
    static const Number value = Read(Srp1024Prime());
    return value;
}
inline bool Zero(const Number& value) {
    uint32_t combined = 0;
    for (auto word : value.word)
        combined |= word;
    return combined == 0;
}
inline bool Less(const Number& a, const Number& b) {
    for (size_t i = 33; i-- > 0;)
        if (a.word[i] != b.word[i])
            return a.word[i] < b.word[i];
    return false;
}
inline Number Select(const Number& a, const Number& b, uint32_t chooseB) {
    Number result;
    const uint32_t mask = 0u - chooseB;
    for (size_t i = 0; i < 33; ++i)
        result.word[i] = (a.word[i] & ~mask) | (b.word[i] & mask);
    return result;
}
inline Number Reduce(const Number& value) {
    Number difference;
    uint64_t borrow = 0;
    for (size_t i = 0; i < 33; ++i) {
        const uint64_t next = static_cast<uint64_t>(value.word[i]) - Prime().word[i] - borrow;
        difference.word[i] = static_cast<uint32_t>(next);
        borrow = next >> 63;
    }
    return Select(difference, value, static_cast<uint32_t>(borrow));
}
inline Number Add(const Number& a, const Number& b) {
    Number result;
    uint64_t carry = 0;
    for (size_t i = 0; i < 33; ++i) {
        const uint64_t next = static_cast<uint64_t>(a.word[i]) + b.word[i] + carry;
        result.word[i] = static_cast<uint32_t>(next);
        carry = next >> 32;
    }
    return Reduce(result);
}
// Fixed-width Montgomery product: a*b/R modulo N, R=2^1024.
inline Number Multiply(const Number& a, const Number& b) {
    struct Product {
        uint32_t word[65] = {};
        ~Product() {
            SecureZeroMemory(word, sizeof(word));
        }
    } product;
    for (size_t i = 0; i < 32; ++i) {
        uint64_t carry = 0;
        for (size_t j = 0; j < 32; ++j) {
            const uint64_t next = static_cast<uint64_t>(a.word[i]) * b.word[j] + product.word[i + j] + carry;
            product.word[i + j] = static_cast<uint32_t>(next);
            carry = next >> 32;
        }
        product.word[i + 32] = static_cast<uint32_t>(carry);
    }
    static const uint32_t inverse = [] {
        uint32_t result = 1;
        for (unsigned i = 0; i < 5; ++i)
            result *= 2u - Prime().word[0] * result;
        return 0u - result;
    }();
    for (size_t i = 0; i < 32; ++i) {
        const uint32_t factor = product.word[i] * inverse;
        uint64_t carry = 0;
        for (size_t j = 0; j < 32; ++j) {
            const uint64_t next = static_cast<uint64_t>(factor) * Prime().word[j] + product.word[i + j] + carry;
            product.word[i + j] = static_cast<uint32_t>(next);
            carry = next >> 32;
        }
        for (size_t j = i + 32; j < 65; ++j) {
            const uint64_t next = static_cast<uint64_t>(product.word[j]) + carry;
            product.word[j] = static_cast<uint32_t>(next);
            carry = next >> 32;
        }
    }
    Number result;
    for (size_t i = 0; i < 33; ++i)
        result.word[i] = product.word[i + 32];
    return Reduce(result);
}
inline const Number& SquareR() {
    static const Number value = [] {
        Number result;
        result.word[0] = 1;
        for (unsigned i = 0; i < 2048; ++i)
            result = Add(result, result);
        return result;
    }();
    return value;
}
inline Number Power(const Number& base, ByteView exponent) {
    Number one;
    one.word[0] = 1;
    Number result = Multiply(one, SquareR());
    const Number factor = Multiply(base, SquareR());
    for (size_t i = 0; i < exponent.size; ++i) {
        for (unsigned bit = 8; bit-- > 0;) {
            const Number squared = Multiply(result, result);
            const Number timesBase = Multiply(squared, factor);
            result = Select(squared, timesBase, (exponent.data[i] >> bit) & 1u);
        }
    }
    return Multiply(result, one);
}
inline Number Product(const Number& a, const Number& b) {
    return Multiply(Multiply(a, SquareR()), b);
}
inline bool Value(ByteView input, Number& out) {
    if (!input.Valid() || !input.size || input.size > 128)
        return false;
    Number value = Read(input);
    if (Zero(value) || !Less(value, Prime()))
        return false;
    out = value;
    return true;
}
inline bool AppendNumber(const Number& value, Buffer& out, bool trim) {
    uint8_t bytes[128];
    Write(value, bytes);
    size_t start = 0;
    if (trim)
        while (start < sizeof(bytes) && bytes[start] == 0)
            ++start;
    Buffer result;
    const bool ok = start < sizeof(bytes) && result.Append(ByteView(bytes + start, sizeof(bytes) - start));
    SecureZeroMemory(bytes, sizeof(bytes));
    if (!ok)
        return false;
    out = std::move(result);
    return true;
}
} // namespace srp_detail
} // namespace glide
