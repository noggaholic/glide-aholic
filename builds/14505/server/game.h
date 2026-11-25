#pragma once
#include "glide/profile/game_messages.h"
#include <memory>

namespace glide {
namespace june14505 {
// Must match the owned actor ID used by this session's construction recipe.
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent);
const GameMessages& ServiceMessages();
std::unique_ptr<GameMessages> MakeGameMessagesForBuild(unsigned build, uint16_t ownedAgent);
const GameMessages* ServiceMessagesForBuild(unsigned build);
} // namespace june14505
} // namespace glide
