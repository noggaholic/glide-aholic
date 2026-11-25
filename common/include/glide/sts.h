#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
constexpr size_t kStsMaxHeader = 16384;
constexpr size_t kStsMaxBody = 1024 * 1024;

struct StsRequest {
    std::string path;
    std::string sequence;
    ByteView body;
    size_t consumed = 0;
};

// Strict Base64 used by native STS Password fields; leaves output unchanged on failure.
bool DecodeStsPassword(const std::string& encoded, std::string& out);

// A completed request borrows its body from the input until it is consumed.
DecodeStatus DecodeStsRequest(ByteView input, StsRequest& out);
bool EncodeStsResponseHeader(const std::string& sequence, unsigned status, const std::string& statusText,
                             size_t bodySize, Buffer& out);
} // namespace glide
