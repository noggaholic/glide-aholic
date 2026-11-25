#pragma once
#include "character_catalog.h"

namespace glide {
namespace april14075 {
bool OwnedActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                uint32_t sequence, Buffer& out, uint32_t karma = 0);
bool PeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
               uint32_t sequence, Buffer& out);
// The active session publishes its own armor; PeerActor above preserves the
// retained empty-inventory reference for exact historical comparisons.
bool EquippedPeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                       uint32_t sequence, Buffer& out);
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out);
bool PlayerControl(uint8_t player, uint16_t agent, Buffer& out);
bool ActivateActor(const Character& character, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace april14075
} // namespace glide
