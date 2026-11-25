#pragma once
#include "glide/profile/game_messages.h"
namespace glide {
namespace july14812 {
PositionResult MotionPosition(ByteView, CatalogPosition&);
FacingResult MotionFacing(ByteView, Facing&);
bool ValidOwnedMotion(ByteView, uint16_t ownedAgent);
RelayResult OwnMotion(ByteView, uint16_t ownedAgent, uint32_t sequence, Buffer&);
bool SyncPosition(const CatalogPosition&, uint16_t agent, uint32_t sequence, Buffer&);
} // namespace july14812
} // namespace glide
