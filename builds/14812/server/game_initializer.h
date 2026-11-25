#pragma once
#include "glide/profile/game_initializer.h"
#include "glide/profile/build_profile.h"
#include <array>
namespace glide {
namespace july14812 {
struct GameInitializer {
    uint32_t association = 0, unwritten = 0, map = 0, context = 0;
    std::array<uint8_t, 16> user{}, character{}, client{};
    uint32_t clientContext = 0;
};
// Original July envelope/fields; unwritten/client values do not grant permission.
bool DecodeGameInitializer(const InitHeader&, GameInitializer&);
bool DecodeGameInitializer(ByteView exactRecord, GameInitializer&);
const GameInitializerCodec& Initializers();
const TransportVariant& Transport();
} // namespace july14812
} // namespace glide
