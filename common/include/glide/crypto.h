#pragma once
#include "glide/buffer.h"

namespace glide {
bool RandomBytes(uint8_t* output, size_t size);
bool Sha256(ByteView input, uint8_t digest[32]);
bool Sha1(ByteView input, uint8_t digest[20]);
bool HmacSha256(ByteView key, ByteView input, uint8_t digest[32]);
bool HmacSha1(ByteView key, ByteView input, uint8_t digest[20]);
bool Aes256Cbc(bool encrypt, const uint8_t key[32], const uint8_t iv[16], ByteView input, uint8_t* output);
bool TlsPrf(ByteView secret, const char* label, ByteView seed, uint8_t* output, size_t size);
} // namespace glide
