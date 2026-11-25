#include "world_actor.h"
#include "profession_state.h"
#include "playable_loadouts.h"
#include "weapon_permissions.h"
#include "glide/profile/character_starting_level.h"
#include <cmath>
#include <utility>
namespace glide {
namespace hot49284 {
namespace {
class Writer {
  public:
    void Word(uint16_t value) {
        valid = valid && bytes.U16(value);
    }
    void Var(uint32_t value) {
        valid = valid && bytes.VarU32(value);
    }
    void Byte(uint8_t value) {
        valid = valid && bytes.U8(value);
    }
    void Float(float value) {
        valid = valid && bytes.F32(value);
    }
    void Bytes(ByteView value) {
        valid = valid && bytes.Append(value);
    }
    bool Finish(Buffer& out) {
        if (!valid)
            return false;
        out = std::move(bytes);
        return true;
    }

  private:
    bool valid = true;
    Buffer bytes;
};
} // namespace
WorldActor::WorldActor() {
    inventoryLocations.fill(-1);
    for (size_t i = 1; i <= 4; ++i) {
        stats[i][0] = 24;
        stats[i][1] = 24;
    }
}
bool BindActor(WorldActor& actor, uint8_t player, uint32_t session) {
    if (!player || !session || session > 0x07ffffff || actor.itemBase)
        return false;
    actor.agent = static_cast<uint16_t>(256 + player);
    actor.itemBase = session * 32;
    return true;
}
bool EncodeWorldActor(const MapEntry& world, const RosterEntry& character, const WorldActor& actor, bool remote,
                      Buffer& out) {
    Buffer validEntry, validSummary, name;
    if (!EncodeMapEntry(world, MapEntryStep::Identity, validEntry) || !EncodeSummary(character.summary, validSummary) ||
        character.name != world.name || world.player > 0x0fffffff || !actor.agent || !actor.sequence ||
        actor.itemBase > 0xffffffe0 || (actor.itemBase & 31))
        return false;
    const auto& summary = character.summary;
    const unsigned profession = summary.profession;
    const Hot49284Loadout* loadout = nullptr;
    for (const auto& candidate : PlayableLoadouts())
        if (candidate.race == summary.appearance[27] && candidate.sex == summary.appearance[29] &&
            candidate.profession == profession)
            loadout = &candidate;
    if (!loadout || !summary.level || summary.level > kMaximumCharacterLevel || summary.map != world.map)
        return false;
    for (size_t slot = 0; slot < 6; ++slot) {
        if (summary.armor[slot] != (actor.inventoryLocations[slot] == -1 ? loadout->skins[slot] : 0))
            return false;
        if (actor.inventoryLocations[slot] != -1)
            for (uint16_t color : summary.colors[slot])
                if (color)
                    return false;
    }
    const float currentHealth = actor.currentHealth == -1 ? actor.health : actor.currentHealth;
    if (!std::isfinite(actor.radius) || actor.radius <= 0 || !std::isfinite(actor.health) || actor.health <= 0 ||
        !std::isfinite(currentHealth) || std::signbit(currentHealth) || currentHealth > actor.health ||
        !std::isfinite(actor.healthRecovery) || actor.healthRecovery < 0 || !actor.endurance ||
        actor.inventorySlots > 160 || !actor.professionMaximum || actor.professionCurrent > actor.professionMaximum)
        return false;
    for (size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(actor.movement[axis]) || actor.movement[axis] <= 0 ||
            !std::isfinite(actor.personality[axis]) || actor.personality[axis] < 0 || actor.personality[axis] > 1)
            return false;
    if (!name.Utf16Z(world.name.c_str(), 19))
        return false;
    static const uint16_t managers[9] = {325, 346, 319, 334, 342, 322, 328, 331, 337};
    unsigned i, j;
    Writer writer;
    if (!remote) {
        writer.Word(736);
        writer.Word(750);
        writer.Word(65);
        writer.Word(actor.agent);
    }
    writer.Word(293);
    writer.Var(world.player);
    writer.Bytes(name.View());
    writer.Bytes(ByteView(character.guid.data(), 16));
    writer.Var(0); /* Original Player constructor flags clear. */
    writer.Word(300);
    writer.Var(world.player);
    writer.Var(0);
    writer.Word(25);
    writer.Word(actor.agent);
    writer.Var(actor.sequence);
    writer.Byte(0x80);
    writer.Var(0);
    writer.Byte(0);
    writer.Byte(0);
    writer.Var(0x30000000u | world.player);
    /* HoT MP_VECTOR3_COORD: three floats plus U32Opt coordinate token.
     * Its native storage is16 bytes, but the zero-token wire form is13. */
    writer.Byte(0x10);
    writer.Byte(1);
    for (float coordinate : world.position)
        writer.Float(coordinate);
    writer.Var(0);
    writer.Word(0);
    writer.Word(0);
    for (i = 0; i < 3; i++)
        writer.Float(actor.movement[i]);
    writer.Float(actor.radius);
    writer.Word(298);
    writer.Var(world.player);
    writer.Var(actor.agent);
    writer.Word(140);
    writer.Var(actor.agent);
    writer.Bytes(ByteView(summary.appearance.data(), 45));
    writer.Byte(summary.level);
    writer.Byte(summary.level);
    writer.Byte((uint8_t)profession);
    writer.Var(0);
    for (i = 0; i < 11; i++)
        for (j = 0; j < 3; j++)
            writer.Word(actor.stats[i][j]);
    for (i = 0; i < 3; i++)
        writer.Float(actor.personality[i]);
    writer.Word(167);
    writer.Var(actor.agent);
    for (i = 0; i < 3; i++)
        writer.Var(actor.endurance);
    writer.Word(180);
    writer.Var(actor.agent);
    writer.Byte(0);
    writer.Float(currentHealth);
    writer.Float(actor.health);
    writer.Float(actor.healthRecovery);
    writer.Word(201);
    writer.Var(actor.agent);
    writer.Word(223);
    writer.Var(actor.agent);
    writer.Var(actor.inventorySlots);
    /* HoT's constructor allocates all five kennel slots itself. Unlike July,
     * S236 rejects a zero pet in slots0..3; do not send the old empty slots. */
    writer.Word(231);
    writer.Var(actor.agent);
    writer.Byte(0);
    writer.Byte(0);
    writer.Byte(0);
    writer.Word(232);
    writer.Var(actor.agent);
    writer.Word(managers[profession - 1]);
    writer.Var(actor.agent);
    if (profession == 7) {
        writer.Byte(1);
        writer.Byte(0);
    } else if (profession == 8) {
        writer.Float(0);
        writer.Float((float)actor.professionMaximum);
    } else if (profession == 9) {
        writer.Byte(2);
        writer.Byte(actor.legends[0]);
        writer.Byte(actor.legends[1]);
        /* S337 creates legends but leaves energy current/maximum at zero.
         * The native meter divides current by maximum. S340 must precede
         * view activation: current, maximum, regeneration, in this order. */
        writer.Word(340);
        writer.Var(actor.agent);
        writer.Float((float)actor.professionCurrent);
        writer.Float((float)actor.professionMaximum);
        writer.Float(5);
        if (actor.legends[0]) {
            writer.Word(341);
            writer.Var(actor.agent);
            writer.Var(actor.legends[0]);
        }
    } else if (profession == 1 || profession == 4 || profession == 5) {
        writer.Float((float)actor.professionMaximum);
        writer.Float(0);
        writer.Float(profession == 4 ? 0 : (float)actor.professionCurrent);
    } else {
        writer.Var(actor.professionMaximum);
        writer.Var(profession == 6 ? actor.professionCurrent : 0);
    }
    if (actor.attunement) {
        if (profession != 6 || actor.attunement > 4 || summary.level < AttunementLevel(actor.attunement))
            return false;
        writer.Word(341);
        writer.Var(actor.agent);
        writer.Var(actor.attunement);
    }
    writer.Word(426);
    writer.Var(actor.agent);
    writer.Word(430);
    writer.Var(actor.agent);
    writer.Byte(255);
    writer.Word(427);
    writer.Var(actor.agent);
    /* Item655 armor and Item666 weapon reports construct typed instances
     * before Character207 equips them. Relative instances1..12 are session
     * policy; optional attribute/upgrade/infusion state is empty. */
    for (i = 0; i < 6; i++)
        if (loadout->armor[i] && actor.inventoryLocations[i] == -1) {
            writer.Word(655);
            writer.Var(actor.itemBase + i + 1);
            writer.Var(loadout->armor[i]);
            writer.Byte(actor.inventoryBound[i] ? static_cast<uint8_t>(remote ? 0x20 : 0x21) : 0);
            writer.Var(summary.armor[i]);
            for (j = 0; j < 4; j++)
                writer.Word(summary.colors[i][j]);
            for (j = 0; j < 3; j++)
                writer.Var(0);
            writer.Byte(0);
            writer.Word(207);
            writer.Var(actor.agent);
            writer.Var(actor.itemBase + i + 1);
            writer.Byte((uint8_t)(i + 2));
        }
    for (i = 0; i < 6; i++)
        if (loadout->weapons[i].item && actor.inventoryLocations[i + 6] == -1) {
            writer.Word(666);
            writer.Var(actor.itemBase + i + 7);
            writer.Var(loadout->weapons[i].item);
            writer.Byte(actor.inventoryBound[i + 6] ? static_cast<uint8_t>(remote ? 0x20 : 0x21) : 0);
            for (j = 0; j < 4; j++)
                writer.Var(0);
            writer.Byte(0);
            writer.Var(0);
            writer.Byte(0);
            writer.Word(207);
            writer.Var(actor.agent);
            writer.Var(actor.itemBase + i + 7);
            writer.Byte(loadout->weapons[i].slot);
        }
    writer.Word(218);
    writer.Var(actor.agent);
    writer.Byte(4);
    writer.Word(202);
    writer.Var(actor.agent);
    /* HoT lazily constructs WardrobeMgr when reporting an absent outfit.
     * AvCharModel requires it even without an outfit or equipped items. */
    writer.Word(278);
    writer.Var(actor.agent);
    /* Local exploration grants original GliderSkinDef1. */
    writer.Word(172);
    writer.Var(actor.agent);
    writer.Word(1);
    writer.Word(305);
    writer.Var(world.player);
    writer.Var(0);
    /* The normal view queries PvP and reward-track managers even on a PvE
     * map. Initialize their native empty state, with absent optional content
     * references. This does not enable PvP simulation or grant rewards. */
    writer.Word(356);
    writer.Var(world.player);
    writer.Var(0);
    writer.Var(0);
    writer.Byte(0);
    for (i = 0; i < 6; i++)
        writer.Var(0);
    writer.Word(398);
    writer.Var(world.player);
    for (i = 0; i < 3; i++)
        writer.Var(0);
    /* Player constructor initializes an empty QuestMgr. Report that state
     * before the view getter asserts IsQuestMgrReported(). */
    writer.Word(377);
    writer.Var(world.player);
    writer.Word(121);
    writer.Var(actor.agent);
    writer.Var(0);
    writer.Word(130);
    writer.Var(actor.agent);
    if (!remote) {
        writer.Word(299);
        writer.Var(world.player);
        writer.Var(actor.agent);
        // S222 selects the preferred sets used by native bag equip actions.
        // S218 only sets the active set; leaving the preference at 11 makes
        // the menu query slot 60 (none) and assert in ChCliInventory.cpp.
        writer.Word(222);
        writer.Var(actor.agent);
        writer.Byte(0);
        writer.Byte(4);
        writer.Word(222);
        writer.Var(actor.agent);
        writer.Byte(1);
        writer.Byte(0);
        // S468 reports this player's innate weapon traits. Without it the
        // native bag refuses to reequip even the profession's starter weapon.
        writer.Word(468);
        writer.Var(world.player);
        const auto& permissions = kWeaponPermissions[profession - 1];
        unsigned attunements = 0;
        if (actor.attunement)
            for (unsigned state = 1; state <= 4; ++state)
                if (summary.level >= AttunementLevel(state))
                    ++attunements;
        writer.Byte(static_cast<uint8_t>(permissions.count + attunements));
        for (unsigned index = 0; index < permissions.count; ++index)
            writer.Var(permissions.traits[index]);
        for (unsigned state = 1; state <= attunements; ++state)
            writer.Var(AttunementTrait(state));
        writer.Byte(0); // No selectable combat traits are unlocked here.
        writer.Byte(permissions.lineCount);
        for (unsigned line = 0; line < permissions.lineCount; ++line)
            writer.Byte(0);
        for (unsigned state = 1; state <= attunements; ++state) {
            writer.Word(416);
            writer.Var(5491 + state);
            writer.Var(world.player);
        }
        if (profession == 9) {
            // Original base selectors without trait1769. Setting the legend pair does not grant these
            // learned-skill bits.
            const unsigned selectors[] = {28085, 28134, 28419, 28494};
            for (unsigned skill : selectors) {
                writer.Word(416);
                writer.Var(skill);
                writer.Var(world.player);
            }
        }
        /* S174 looks up the controlled Player implicitly; it must follow S299. */
        writer.Word(174);
        writer.Word(1);
    }
    return writer.Finish(out);
}
bool ActivateWorldActor(const MapEntry& world, uint16_t agent, uint32_t sequence, bool remote, Buffer& out) {
    Buffer validEntry;
    if (!agent || !sequence || !EncodeMapEntry(world, MapEntryStep::Identity, validEntry))
        return false;
    Writer writer;
    writer.Word(34);
    writer.Word(agent);
    writer.Var(sequence);
    writer.Byte(0x50);
    writer.Byte(0);
    writer.Byte(12);
    for (float coordinate : world.position)
        writer.Float(coordinate);
    writer.Var(0);
    writer.Word(0);
    writer.Word(0);
    if (!remote)
        writer.Word(726);
    return writer.Finish(out);
}
bool AnnouncePeer(const MapEntry& world, const RosterEntry& character, const WorldActor& actor, Buffer& out) {
    if (!world.player || world.player >= 255 || actor.agent != 256 + world.player || !actor.itemBase ||
        actor.sequence == UINT32_MAX)
        return false;
    Buffer result, activation;
    if (!EncodeWorldActor(world, character, actor, true, result) ||
        !ActivateWorldActor(world, actor.agent, actor.sequence + 1, true, activation) ||
        !result.Append(activation.View()))
        return false;
    out = std::move(result);
    return true;
}
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!player || player == 255 || agent != 256u + player || !sequence)
        return false;
    Buffer result;
    if (!result.U16(29) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x80) || !result.U16(295) ||
        !result.VarU32(player))
        return false;
    out = std::move(result);
    return true;
}
} // namespace hot49284
} // namespace glide
