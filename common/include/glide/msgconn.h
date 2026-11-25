#pragma once
#include "glide/buffer.h"

namespace glide {
enum class KeyExchangeRecord { LegacyFixed66, SizedAligned };
struct InitHeader {
    uint16_t a = 0, b = 0;
    uint32_t build = 0, program = 0;
    ByteView extension;
    size_t consumed = 0;
};

// Parses the shared envelope. Each build still validates a/b, program and route.
DecodeStatus DecodeInit(ByteView input, InitHeader& out);
// Server-to-client uncompressed frame: u16 compressed size (zero), u16 raw size.
bool EncodeRawFrame(ByteView payload, Buffer& out);
} // namespace glide
