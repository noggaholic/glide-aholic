#pragma once
#include "glide/buffer.h"

namespace glide {
namespace china167409 {
struct AuthRequest {
    uint16_t id = 0;
    size_t consumed = 0;
    // Borrowed complete payload, excluding the message ID; valid while input stays alive.
    ByteView payload;
};
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out);
} // namespace china167409
} // namespace glide
