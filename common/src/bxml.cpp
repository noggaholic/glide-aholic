#include "glide/bxml.h"
#include <cstring>

namespace glide {
namespace {
uint32_t U32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
uint32_t Rotate(uint32_t v, unsigned n) {
    return (v << n) | (v >> (32 - n));
}
void Put(uint8_t* p, uint32_t v) {
    for (unsigned i = 0; i < 4; ++i)
        p[i] = static_cast<uint8_t>(v >> (i * 8));
}
} // namespace
bool ValidateBxml(ByteView bytes, std::string& error) {

    error = "Invalid BXML2 header or version";
    if (!bytes.Valid() || bytes.size < 16 || std::memcmp(bytes.data, "bxm2", 4) || bytes.data[4] || bytes.data[5])
        return false;
    const size_t pool = U32(bytes.data + 6);
    error = "Invalid BXML string pool";
    if (pool < 16 || pool >= bytes.size || bytes.data[bytes.size - 1])
        return false;
    size_t at = 10, pending = 1;
    while (at < pool) {
        error = "Invalid BXML record at " + std::to_string(at);
        if (!pending || pool - at < 2)
            return false;
        const unsigned flags = bytes.data[at];
        const size_t header = flags & 0x80 ? 9 : 2;
        if (pool - at < header)
            return false;
        const size_t attributes = flags & 0x80 ? U32(bytes.data + at + 1) : flags & 0x3f;
        const size_t children = flags & 0x80 ? U32(bytes.data + at + 5) : bytes.data[at + 1];
        if (attributes > (pool - at - header) / 8)
            return false;
        const size_t references = 1 + ((flags >> 6) & 1) + attributes * 2;
        if (references > (pool - at - header) / 4)
            return false;
        const size_t length = header + references * 4;
        for (size_t i = 0; i < references; ++i) {
            const size_t offset = U32(bytes.data + at + header + i * 4) >> 1;
            if (offset >= bytes.size - pool || !std::memchr(bytes.data + pool + offset, 0, bytes.size - pool - offset))
                return false;
        }
        --pending;
        if (children > (pool - at - length) / 6 || pending > (pool - at - length) / 6 - children)
            return false;
        pending += children;
        at += length;
    }
    if (pending) {
        error = "Missing BXML child records";
        return false;
    }
    error.clear();
    return true;
}
bool DeriveContentKey(ByteView seed, uint8_t key[20]) {
    // Existing beta content derivation; RC4 itself uses the shared Rc4 class.
    if (!seed.Valid() || !seed.size || !key)
        return false;
    uint8_t folded[20];
    size_t i = 0;
    for (; i < 20; ++i)
        folded[i] = seed.data[i % seed.size];
    for (; i < seed.size; ++i)
        folded[i % 20] ^= seed.data[i];
    const uint32_t a = U32(folded) + 0x9fb498b3u;
    const uint32_t b = Rotate(a, 5) + 0x66b0cd0du + U32(folded + 4);
    const uint32_t c = (~(a & 0x22222222u) & 0x7bf36ae2u) + 0xf33d5697u + Rotate(b, 5) + U32(folded + 8);
    const uint32_t r0 = Rotate(a, 30);
    const uint32_t d = (((r0 ^ 0x59d148c0u) & b) ^ 0x59d148c0u) + 0xd675e47bu + Rotate(c, 5) + U32(folded + 12);
    const uint32_t r1 = Rotate(b, 30);
    const uint32_t next = (((r0 ^ r1) & c) ^ r0) + U32(folded + 16) + 0xb453c259u + Rotate(d, 5) + U32(folded);
    Put(key, next);
    Put(key + 4, U32(folded + 4) + d);
    Put(key + 8, U32(folded + 8) + Rotate(c, 30));
    Put(key + 12, U32(folded + 12) + r1);
    Put(key + 16, U32(folded + 16) + r0);
    return true;
}
} // namespace glide
