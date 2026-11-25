#pragma once
#include "glide/profile/game_messages.h"
#include <memory>
namespace glide {
namespace july14812 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent, uint16_t serverPort);
const GameMessages& ServiceMessages();
} // namespace july14812
} // namespace glide
