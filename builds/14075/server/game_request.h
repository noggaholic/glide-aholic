#pragma once
#include "../../14505/server/game_request.h"

namespace glide {
namespace april14075 {
using GameRequest = june14505::GameRequest;
using GameRequestKind = june14505::GameRequestKind;
DecodeStatus DecodeGameRequest(ByteView input, GameRequest& out);
} // namespace april14075
} // namespace glide
