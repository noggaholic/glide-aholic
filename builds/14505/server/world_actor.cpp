#include "world_actor.h"
#include "inventory_state.h"
#include "mission.h"
#include "world_managers.h"
#include "world_protocol.h"
#include "glide/profile/june_health.h"
#include <cmath>

namespace glide {
namespace june14505 {
namespace {
bool Position(const Character& character, float (&position)[3]) {
    position[0] = character.x * 32;
    position[1] = character.y * 32;
    position[2] = character.z == 0 ? 0 : -character.z * 32;
    for (float value : position)
        if (!std::isfinite(value))
            return false;
    return true;
}
bool Transform(Buffer& out, const float (&position)[3]) {
    for (float value : position)
        if (!out.F32(value))
            return false;
    return out.F32(0) && out.U16(0) && out.U16(45153);
}
bool Actor(unsigned build, const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
           uint32_t sequence, bool owned, Buffer& out, ByteView publishedEquipment = ByteView(),
           bool equipmentProvided = false, uint32_t karma = 0) {
    float position[3];
    RosterCharacter roster;
    std::string error;
    if (!WorldProtocolBuild(build) || catalog.Build() != build || !player || !agent || !sequence || karma > 1000000 ||
        (build == 14075 && (player == 255 || agent != 0x100u + player)) || !Position(character, position) ||
        !catalog.Resolve(character, roster, error))
        return false;
    unsigned effectiveLevel = character.level;
    const auto effective = character.profileFields.find("effective_level");
    if (effective != character.profileFields.end() &&
        (!ParseUnsigned(effective->second, 80, effectiveLevel) || !effectiveLevel))
        return false;
    float health = 245.0f, third = 24.5f;
    if (beta::LegacyHealthBuild(build)) {
        if (!modern::ReadHealthState(character, health, beta::kJuneMaximumHealth))
            return false;
        const auto saved = character.profileFields.find("health_state");
        if (saved != character.profileFields.end() && !saved->second.empty())
            third = 0;
    }
    Buffer result;
    if (owned &&
        (!MissionPrepareForBuild(build, result) || !result.U16(WorldMessage(build, 0x002f)) || !result.U16(agent)))
        return false;
    if (!result.U16(WorldMessage(build, 0x00b3)) || !result.VarU32(player) ||
        !result.Utf16Z(character.name.c_str(), 19) || !result.Append(ByteView(character.guid.bytes, 16)) ||
        !result.U16(WorldMessage(build, owned ? 0x00b8 : 0x00bb)) || !result.VarU32(player) || !result.VarU32(0))
        return false;
    // Establish local control before constructing the Agent's controller graph.
    if (!result.U16(WorldMessage(build, 0x000c)) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(0x80) || !result.VarU32(0) || !result.U8(0) || !result.U8(0) ||
        !result.VarU32(0x30000000u | player) || !result.U8(0x10) || !result.U8(49) || !Transform(result, position) ||
        !result.F32(294.0f) || !result.F32(2.0944f) || !result.F32(70.0f) || !result.F32(24.0f))
        return false;
    if (!result.U16(WorldMessage(build, 0x00b6)) || !result.VarU32(player) || !result.VarU32(agent) ||
        !result.U16(WorldMessage(build, 0x0121)) || !result.VarU32(agent) || !result.VarU32(0) || !result.VarU32(14) ||
        !result.U16(WorldMessage(build, 0x003d)) || !result.VarU32(agent) || !result.VarU32(0) ||
        !result.U16(WorldMessage(build, 0x0048)) || !result.VarU32(agent) ||
        !result.Append(ByteView(roster.appearance.data(), 45)) || !result.U8(roster.level) ||
        !result.U8(static_cast<uint8_t>(effectiveLevel)) || !result.U8(roster.profession) || !result.VarU32(0) ||
        !result.VarU32(karma))
        return false;
    for (unsigned i = 0; i < 8; ++i) {
        const uint16_t value = i >= 1 && i <= 4 ? 24 : 0;
        if (!result.U16(value) || !result.U16(value) || !result.U16(0))
            return false;
    }
    if (!result.F32(0.25f) || !result.F32(0.5f) || !result.F32(0.25f))
        return false;
    for (uint16_t id : {uint16_t(0x0046), uint16_t(0x0047), uint16_t(0x005b)})
        if (!result.U16(WorldMessage(build, id)) || !result.VarU32(agent) || !result.U8(0))
            return false;
    if (owned && (!result.U16(WorldMessage(build, 0x0060)) || !result.VarU32(agent) || !result.VarU32(100) ||
                  !result.VarU32(100) || !result.VarU32(100)))
        return false;
    if (!result.U16(WorldMessage(build, 0x0068)) || !result.VarU32(agent) || !result.U8(0) || !result.F32(health) ||
        !result.F32(245.0f) || !result.F32(third) || !result.U16(WorldMessage(build, 0x0076)) || !result.VarU32(agent))
        return false;
    if (!owned) {
        Buffer managers, equipment, activate;
        // Preserve the original empty April reference unless its profile supplies
        // equipment built from the verified April Item/Char registrations.
        if (sequence == UINT32_MAX || !publishedEquipment.Valid() ||
            (equipmentProvided && !equipment.Append(publishedEquipment)) ||
            (!equipmentProvided && build != 14075 &&
             !InitialInventoryEquipment(catalog, character, player, agent, equipment, false)) ||
            !PeerManagersForBuild(build, agent, roster.profession, effectiveLevel, managers, equipment.View()) ||
            !ActivateActorForBuild(build, character, agent, sequence + 1, activate) ||
            !result.Append(managers.View()) || !result.Append(activate.View()))
            return false;
    }
    out = std::move(result);
    return true;
}
} // namespace
bool InitialInventoryBagForBuild(unsigned build, uint8_t player, uint16_t agent, Buffer& out) {
    if (!WorldProtocolBuild(build) || !player || player == 255 || agent != 0x100u + player)
        return false;
    // Preserve the existing April allocation and count/report/slot order.
    // Native ItemDef lookup uses DataId 2532; the content UID differs by build.
    const uint32_t instance = static_cast<uint32_t>(player) * 32 + 31;
    Buffer result;
    if (!result.U16(WorldMessage(build, 131)) || !result.VarU32(agent) || !result.U8(1) ||
        !result.U16(WorldMessage(build, 418)) || !result.VarU32(instance) || !result.VarU32(2532) || !result.U8(0) ||
        !result.U8(0) || !result.U16(WorldMessage(build, 130)) || !result.VarU32(agent) || !result.VarU32(0) ||
        !result.VarU32(instance))
        return false;
    out = std::move(result);
    return true;
}
bool StartingEquipment(const RosterCharacter& character, uint8_t player, uint16_t agent, Buffer& out) {
    Buffer result;
    if (!ArmorEquipment(character, player, agent, result) || character.weapons.size() > 2)
        return false;
    unsigned seen = 0;
    for (size_t i = 0; i < character.weapons.size(); ++i) {
        const auto& weapon = character.weapons[i];
        const unsigned bit = weapon.slot == 24 ? 1u : weapon.slot == 29 ? 2u : 0u;
        if (!weapon.item || !bit || (seen & bit))
            return false;
        seen |= bit;
        // Armor uses offsets 1..6; weapons use 7..8 in the same per-player block.
        const uint32_t instance = static_cast<uint32_t>(player) * 32 + 7 + static_cast<uint32_t>(i);
        // Optional appearance/upgrade and constructor-default report fields remain zero.
        if (!result.U16(419) || !result.VarU32(instance) || !result.VarU32(weapon.item) || !result.U8(0) ||
            !result.VarU32(0) || !result.VarU32(0) || !result.U8(0) || !result.VarU32(0) || !result.U16(123) ||
            !result.VarU32(agent) || !result.VarU32(instance) || !result.U8(weapon.slot))
            return false;
    }
    out = std::move(result);
    return true;
}
bool ArmorEquipment(const RosterCharacter& character, uint8_t player, uint16_t agent, Buffer& out) {
    return ArmorEquipmentForBuild(14505, character, player, agent, out);
}
bool ArmorEquipmentForBuild(unsigned build, const RosterCharacter& character, uint8_t player, uint16_t agent,
                            Buffer& out) {
    if (!WorldProtocolBuild(build) || !player || player == 255 || agent != 0x100u + player)
        return false;
    // Both installed equipment enums have this summary order.
    const uint16_t report = build == 14075 ? 321 : 408, attach = build == 14075 ? 98 : 123;
    const uint8_t slots[] = {2, 3, 5, 4, 6, 7};
    Buffer result;
    for (size_t i = 0; i < character.armor.size(); ++i) {
        if (!character.armor[i])
            continue;
        const uint32_t instance = static_cast<uint32_t>(player) * 32 + static_cast<uint32_t>(i) + 1;
        if (!result.U16(report) || !result.VarU32(instance) || !result.VarU32(character.armor[i]) || !result.U8(0) ||
            !result.VarU32(0))
            return false;
        // Native type15 is a fixed array: four u16 colors, no count prefix.
        for (uint16_t dye : character.dyes[i])
            if (!result.U16(dye))
                return false;
        if (!result.VarU32(0) || !result.U16(attach) || !result.VarU32(agent) || !result.VarU32(instance) ||
            !result.U8(slots[i]))
            return false;
    }
    out = std::move(result);
    return true;
}
bool OwnedActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
                uint32_t sequence, Buffer& out, uint32_t karma) {
    return (catalog.Build() == 14422 || catalog.Build() == 14505) &&
           OwnedActorForBuild(catalog.Build(), catalog, character, player, agent, sequence, out, karma);
}
bool PeerActor(const CharacterCatalog& catalog, const Character& character, uint8_t player, uint16_t agent,
               uint32_t sequence, Buffer& out) {
    return (catalog.Build() == 14422 || catalog.Build() == 14505) &&
           PeerActorForBuild(catalog.Build(), catalog, character, player, agent, sequence, out);
}
bool RemovePeerForBuild(unsigned build, uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!WorldProtocolBuild(build) || !player || !agent || !sequence ||
        (build == 14075 && (player == 255 || agent != 0x100u + player)))
        return false;
    Buffer result;
    if (!result.U16(WorldMessage(build, 0x0010)) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(0x80) || !result.U16(WorldMessage(build, 0x00b4)) || !result.VarU32(player))
        return false;
    out = std::move(result);
    return true;
}
bool PlayerControlForBuild(unsigned build, uint8_t player, uint16_t agent, Buffer& out) {
    if (!WorldProtocolBuild(build) || !player || !agent ||
        (build == 14075 && (player == 255 || agent != 0x100u + player)))
        return false;
    Buffer result;
    if (!result.U16(WorldMessage(build, 0x00b7)) || !result.VarU32(player) || !result.VarU32(agent))
        return false;
    out = std::move(result);
    return true;
}
bool ActivateActorForBuild(unsigned build, const Character& character, uint16_t agent, uint32_t sequence, Buffer& out) {
    float position[3];
    if (!WorldProtocolBuild(build) || !agent || !sequence || !Position(character, position))
        return false;
    Buffer result;
    if (!result.U16(WorldMessage(build, 0x0013)) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(0x40) || !result.U8(1) || !result.U8(6) || !Transform(result, position))
        return false;
    out = std::move(result);
    return true;
}
bool OwnedActorForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character, uint8_t player,
                        uint16_t agent, uint32_t sequence, Buffer& out, uint32_t karma) {
    // Native ChCliCoreStats carries experience, then karma (maximum 1,000,000).
    return Actor(build, catalog, character, player, agent, sequence, true, out, ByteView(), false, karma);
}
bool PeerActorForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character, uint8_t player,
                       uint16_t agent, uint32_t sequence, Buffer& out) {
    return Actor(build, catalog, character, player, agent, sequence, false, out);
}
bool PeerActorWithEquipmentForBuild(unsigned build, const CharacterCatalog& catalog, const Character& character,
                                    uint8_t player, uint16_t agent, uint32_t sequence, ByteView equipment,
                                    Buffer& out) {
    return Actor(build, catalog, character, player, agent, sequence, false, out, equipment, true);
}
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out) {
    return RemovePeerForBuild(14505, player, agent, sequence, out);
}
bool PlayerControl(uint8_t player, uint16_t agent, Buffer& out) {
    return PlayerControlForBuild(14505, player, agent, out);
}
bool ActivateActor(const Character& character, uint16_t agent, uint32_t sequence, Buffer& out) {
    return ActivateActorForBuild(14505, character, agent, sequence, out);
}
} // namespace june14505
} // namespace glide
