#pragma once
#include "../../14505/server/game.h"

namespace glide {
namespace april14075 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent);
const GameMessages& ServiceMessages();
} // namespace april14075
} // namespace glide
