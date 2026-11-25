#pragma once
#include "glide/profile/game_messages.h"
#include <memory>
namespace glide {
namespace hot49284 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent, uint16_t serverPort);
const GameMessages& ServiceMessages();
} // namespace hot49284
} // namespace glide
