#include "world_actor.h"
#include "inventory_state.h"
#include "../../14505/server/world_actor.h"

namespace glide {
namespace april14075 {
bool OwnedActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                uint32_t sequence, Buffer& out, uint32_t karma) {
    return june14505::OwnedActorForBuild(14075, catalog, character, player, agent, sequence, out, karma);
}
bool PeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
               uint32_t sequence, Buffer& out) {
    return june14505::PeerActorForBuild(14075, catalog, character, player, agent, sequence, out);
}
bool EquippedPeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                       uint32_t sequence, Buffer& out) {
    june14505::RosterCharacter roster;
    std::string error;
    Buffer equipment;
    return catalog.Resolve(character, roster, error) &&
           InitialInventoryEquipment(catalog, character, player, agent, equipment, false) &&
           june14505::PeerActorWithEquipmentForBuild(14075, catalog, character, player, agent, sequence,
                                                     equipment.View(), out);
}
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out) {
    return june14505::RemovePeerForBuild(14075, player, agent, sequence, out);
}
bool PlayerControl(uint8_t player, uint16_t agent, Buffer& out) {
    return june14505::PlayerControlForBuild(14075, player, agent, out);
}
bool ActivateActor(const Character& character, uint16_t agent, uint32_t sequence, Buffer& out) {
    return june14505::ActivateActorForBuild(14075, character, agent, sequence, out);
}
} // namespace april14075
} // namespace glide
