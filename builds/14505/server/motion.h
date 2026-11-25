#pragma once
#include "glide/buffer.h"
#include <array>

namespace glide {
namespace june14505 {
struct MotionMessage {
    uint16_t canonicalId = 0;
    uint16_t id = 0, agent = 0, wordA = 0, wordB = 0, optionalWord = 0;
    uint32_t sequence = 0, tick = 0, flags = 0, token = 0, optionalValue = 0;
    uint8_t state = 0, mode = 0;
    bool hasOptional = false;
    float amount = 0;
    std::array<float, 4> position{}, optionalVector{};
    std::array<float, 3> vectorA{}, vectorB{};
    ByteView input; // Borrowed bytes, valid while the source input remains unchanged.
    size_t consumed = 0;
};
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out);
DecodeStatus DecodeMotionForBuild(ByteView input, unsigned build, MotionMessage& out);
} // namespace june14505
} // namespace glide
