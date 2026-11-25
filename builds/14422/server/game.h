#pragma once
#include "glide/profile/game_messages.h"
#include <memory>

namespace glide {
namespace june14422 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent);
const GameMessages& ServiceMessages();
} // namespace june14422
} // namespace glide
