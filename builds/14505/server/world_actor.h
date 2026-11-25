#pragma once
#include "character_catalog.h"

namespace glide {
namespace june14505 {
// Initial owned actor only. Managers/finalization must follow before player control.
bool OwnedActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                uint32_t sequence, Buffer& out, uint32_t karma = 0);
bool StartingEquipment(const RosterCharacter& character, uint8_t player, uint16_t agent, Buffer& out);
bool InitialInventoryBagForBuild(unsigned build, uint8_t player, uint16_t agent, Buffer& out);
bool ArmorEquipment(const RosterCharacter& character, uint8_t player, uint16_t agent, Buffer& out);
// Item and Char registrations are verified independently for each build.
bool ArmorEquipmentForBuild(unsigned build, const RosterCharacter& character, uint8_t player, uint16_t agent,
                            Buffer& out);
bool PlayerControl(uint8_t player, uint16_t agent, Buffer& out);
// Complete remote announcement, without local control or Mission messages.
bool PeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
               uint32_t sequence, Buffer& out);
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out);
// Send after the client's WorldBuilt confirmation, before MissionBegin.
bool ActivateActor(const Character& character, uint16_t agent, uint32_t sequence, Buffer& out);
bool OwnedActorForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character, uint8_t player,
                        uint16_t agent, uint32_t sequence, Buffer& out, uint32_t karma = 0);
bool PeerActorForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character, uint8_t player,
                       uint16_t agent, uint32_t sequence, Buffer& out);
bool PeerActorWithEquipmentForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character,
                                    uint8_t player, uint16_t agent, uint32_t sequence, ByteView equipment, Buffer& out);
bool RemovePeerForBuild(unsigned build, uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out);
bool PlayerControlForBuild(unsigned build, uint8_t player, uint16_t agent, Buffer& out);
bool ActivateActorForBuild(unsigned build, const Character& character, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace june14505
} // namespace glide
