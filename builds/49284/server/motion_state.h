#pragma once
#include "glide/profile/game_messages.h"
namespace glide {
namespace hot49284 {
PositionResult MotionPosition(ByteView, CatalogPosition& out);
FacingResult MotionFacing(ByteView, Facing& out);
// ownedAgent belongs to the authorized world connection; never trust the request ID.
RelayResult OwnMotion(ByteView, uint16_t ownedAgent, uint32_t sequence, Buffer& out);
bool SyncPosition(const CatalogPosition&, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace hot49284
} // namespace glide
