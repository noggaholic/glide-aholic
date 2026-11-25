#pragma once
#include "glide/msgconn.h"
#include "glide/profile/game_initializer.h"
#include <array>
namespace glide {
namespace hot49284 {
struct GameInitializer {
    uint32_t association = 0, unwritten = 0, map = 0, context = 0;
    std::array<uint8_t, 16> user{}, character{}, client{};
    uint32_t clientContext = 0;
};
// Parsing does not authorize a join.
bool DecodeGameInitializer(const InitHeader&, GameInitializer&);
bool DecodeGameInitializer(ByteView exactRecord, GameInitializer&);
const GameInitializerCodec& Initializers();
} // namespace hot49284
} // namespace glide
