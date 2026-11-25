#include "world_actor.h"
#include "inventory_state.h"
#include "world_maps.h"
#include "glide/profile/beta_health.h"
#include <algorithm>
#include <cmath>
#include <utility>
namespace glide {
namespace july14812 {
namespace {
class Writer {
    Buffer bytes_;
    bool ok_ = true;

  public:
    void Word(uint16_t v) {
        ok_ = ok_ && bytes_.U16(v);
    }
    void Var(uint32_t v) {
        ok_ = ok_ && bytes_.VarU32(v);
    }
    void Byte(uint8_t v) {
        ok_ = ok_ && bytes_.U8(v);
    }
    void Float(float v) {
        ok_ = ok_ && bytes_.F32(v);
    }
    void Raw(ByteView v) {
        ok_ = ok_ && bytes_.Append(v);
    }
    void Name(const std::string& v) {
        ok_ = ok_ && bytes_.Utf16Z(v.c_str(), 19);
    }
    bool Finish(Buffer& out) {
        if (!ok_)
            return false;
        out = std::move(bytes_);
        return true;
    }
};
bool Valid(const MapEntry& world, const RosterEntry& character, const WorldActor& a) {
    const auto& s = character.summary;
    if (!ValidMapEntry(world) || world.player > 0x0fffffff || !a.agent || !a.sequence || s.profession < 1 ||
        s.profession > 8 || !s.level || s.level > 80 || s.map != world.map || character.name != world.name ||
        !std::isfinite(a.radius) || a.radius <= 0 || !std::isfinite(a.health) || a.health < 0 ||
        !std::isfinite(a.maximumHealth) || a.maximumHealth <= 0 || a.health > a.maximumHealth ||
        !std::isfinite(a.healthRecovery) || a.healthRecovery < 0 || !a.endurance || !a.professionMaximum ||
        a.professionCurrent > a.professionMaximum || a.inventorySlots > 160 || a.weaponSetPresent > 1 ||
        (a.weaponSetPresent && a.weaponSet >= 11) || a.professionStatePresent > 1 ||
        (a.professionStatePresent && a.professionState >= 9) || a.learnedSkills.size() > MaximumStarterSkills ||
        a.automaticTraits.size() > 4 || a.karma > 1000000)
        return false;
    for (size_t i = 0; i < a.automaticTraits.size(); ++i) {
        const auto trait = a.automaticTraits[i];
        if (s.profession != 6 || trait < 240 || trait > 243 || (trait == 241 && s.level < 3) ||
            (trait == 242 && s.level < 6))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (a.automaticTraits[j] == trait)
                return false;
    }
    for (size_t i = 0; i < 3; ++i)
        if (!std::isfinite(a.movement[i]) || a.movement[i] <= 0 || !std::isfinite(a.personality[i]) ||
            a.personality[i] < 0 || a.personality[i] > 1)
            return false;
    for (size_t i = 0; i < 6; ++i)
        if (a.armor[i].instance) {
            if (!s.armor[i] || a.armor[i].slot >= 59)
                return false;
            for (size_t j = 0; j < i; ++j)
                if (a.armor[j].instance &&
                    (a.armor[j].instance == a.armor[i].instance || a.armor[j].slot == a.armor[i].slot))
                    return false;
        }
    for (size_t i = 0; i < 6; ++i)
        if (a.weapons[i].instance) {
            const auto& w = a.weapons[i];
            if (!w.item || !((w.slot >= 24 && w.slot <= 25) || (w.slot >= 29 && w.slot <= 32)))
                return false;
            for (const auto& armor : a.armor)
                if (armor.instance && (armor.instance == w.instance || armor.slot == w.slot))
                    return false;
            for (size_t j = 0; j < i; ++j)
                if (a.weapons[j].instance && (a.weapons[j].instance == w.instance || a.weapons[j].slot == w.slot))
                    return false;
        }
    for (size_t i = 0; i < a.learnedSkills.size(); ++i) {
        if (!a.learnedSkills[i])
            return false;
        for (size_t j = 0; j < i; ++j)
            if (a.learnedSkills[i] == a.learnedSkills[j])
                return false;
    }
    return true;
}
} // namespace
bool PrepareActor(const Character& saved, const CharacterCatalog& catalog, uint32_t sequence, WorldActor& out) {
    RosterEntry character;
    std::string error;
    bool assigned = false;
    for (uint8_t byte : saved.guid.bytes)
        assigned = assigned || byte != 0;
    InventoryState inventory;
    if (!sequence || !assigned || !catalog.ResolveOriginal(saved, character, error) ||
        !LoadInventoryState(saved, catalog, inventory, error))
        return false;
    const auto& s = character.summary;
    const auto* recipe = catalog.FindForCharacter(saved);
    if (!recipe || !catalog.Maps().Find(s.map))
        return false;
    WorldActor actor;
    actor.sequence = sequence;
    if (!modern::ReadHealthState(saved, actor.health, beta::kJulyMaximumHealth))
        return false;
    // Local level-one simulation policy.
    for (size_t i = 1; i <= 4; ++i)
        actor.stats[i][0] = actor.stats[i][1] = 24;
    for (size_t i = 0; i < 6; ++i)
        if (s.armor[i] && inventory.locations[i] == -1) {
            actor.armor[i].instance = static_cast<uint32_t>(i + 1);
            actor.armor[i].slot = static_cast<uint8_t>(i + 2);
            actor.armor[i].flags = static_cast<uint8_t>(inventory.bound[i] ? 0x21 : 0);
        }
    for (size_t i = 0; i < recipe->weapons.size(); ++i) {
        if (inventory.locations[i + 6] != -1)
            continue;
        actor.weapons[i].instance = static_cast<uint32_t>(7 + i);
        actor.weapons[i].item = recipe->weapons[i].item;
        actor.weapons[i].slot = recipe->weapons[i].slot;
        actor.weapons[i].flags = static_cast<uint8_t>(inventory.bound[i + 6] ? 0x21 : 0);
    }
    actor.weaponSetPresent = 1;
    actor.weaponSet = recipe->weaponSet;
    if (!SavedProfessionState(saved, s.profession, s.level, actor.professionState))
        return false;
    actor.professionStatePresent = actor.professionState ? 1 : 0;
    actor.learnedSkills = recipe->skills;
    // July automatic Display traits: the original profession palette uses
    // these as conditions, independently of the learned weapon skills.
    if (recipe->skillLoadout && s.profession == 6) {
        // BuffSkill variant13417 is omitted by the initial recipe exporter.
        // Learn it while leaving its original availability condition intact.
        if (std::find(actor.learnedSkills.begin(), actor.learnedSkills.end(), 13417u) == actor.learnedSkills.end())
            actor.learnedSkills.push_back(13417u);
        actor.automaticTraits = {243, 240};
        if (s.level >= 3)
            actor.automaticTraits.push_back(241);
        if (s.level >= 6)
            actor.automaticTraits.push_back(242);
    }
    out = std::move(actor);
    return true;
}
bool BindActor(WorldActor& actor, uint8_t player, uint32_t session) {
    if (!player || !session || session > 0x07ffffff)
        return false;
    for (const auto& armor : actor.armor)
        if (armor.instance >= 32)
            return false;
    for (const auto& weapon : actor.weapons)
        if (weapon.instance >= 32)
            return false;
    WorldActor result = actor;
    result.agent = static_cast<uint16_t>(256 + player);
    const uint32_t base = session * 32;
    for (auto& armor : result.armor)
        if (armor.instance)
            armor.instance += base;
    for (auto& weapon : result.weapons)
        if (weapon.instance)
            weapon.instance += base;
    actor = std::move(result);
    return true;
}
bool EncodeWorldActor(const MapEntry& world, const RosterEntry& character, const WorldActor& a, bool remote,
                      Buffer& out) {
    if (!Valid(world, character, a))
        return false;
    const auto& s = character.summary;
    Writer w;
    if (!remote) {
        w.Word(472);
        w.Word(478);
        w.Word(47);
        w.Word(a.agent);
    }
    w.Word(179);
    w.Var(world.player);
    w.Name(world.name);
    w.Raw(ByteView(character.guid.data(), 16));
    w.Word(184);
    w.Var(world.player);
    w.Var(0);
    w.Word(12);
    w.Word(a.agent);
    w.Var(a.sequence);
    w.Byte(0x80);
    w.Var(0);
    w.Byte(0);
    w.Byte(0);
    w.Var(0x30000000u | world.player);
    w.Byte(0x10);
    w.Byte(1);
    for (float v : world.position)
        w.Float(v);
    w.Float(0);
    w.Word(0);
    w.Word(0);
    for (float v : a.movement)
        w.Float(v);
    w.Float(a.radius);
    w.Word(182);
    w.Var(world.player);
    w.Var(a.agent);
    w.Word(72);
    w.Var(a.agent);
    w.Raw(ByteView(s.appearance.data(), 45));
    w.Byte(s.level);
    w.Byte(s.level);
    w.Byte(s.profession);
    w.Var(0);
    w.Var(remote ? 0 : a.karma);
    for (const auto& stat : a.stats)
        for (uint16_t value : stat)
            w.Word(value);
    for (float v : a.personality)
        w.Float(v);
    w.Word(96);
    w.Var(a.agent);
    for (unsigned i = 0; i < 3; ++i)
        w.Var(a.endurance);
    w.Word(104);
    w.Var(a.agent);
    w.Byte(0);
    w.Float(a.health);
    w.Float(a.maximumHealth);
    w.Float(a.healthRecovery);
    w.Word(118);
    w.Var(a.agent);
    w.Word(135);
    w.Var(a.agent);
    w.Var(a.inventorySlots);
    w.Word(142);
    w.Var(a.agent);
    w.Byte(0);
    w.Byte(0);
    w.Byte(0);
    for (uint8_t i = 0; i < 4; ++i) {
        w.Word(147);
        w.Var(a.agent);
        w.Byte(i);
        w.Word(0);
        w.Var(0);
        w.Var(0);
    }
    w.Word(143);
    w.Var(a.agent);
    static const uint16_t managers[] = {204, 221, 198, 213, 217, 201, 207, 210};
    w.Word(managers[s.profession - 1]);
    w.Var(a.agent);
    if (s.profession == 7) {
        w.Byte(1);
        w.Byte(0);
    } else if (s.profession == 8) {
        w.Float(0);
        w.Float(static_cast<float>(a.professionMaximum));
    } else if (s.profession == 1 || s.profession == 4 || s.profession == 5) {
        w.Float(static_cast<float>(a.professionMaximum));
        w.Float(0);
        w.Float(s.profession == 4 ? 0 : static_cast<float>(a.professionCurrent));
    } else {
        w.Var(a.professionMaximum);
        w.Var(s.profession == 6 ? a.professionCurrent : 0);
    }
    w.Word(268);
    w.Var(a.agent);
    w.Word(272);
    w.Var(a.agent);
    w.Byte(255);
    w.Word(269);
    w.Var(a.agent);
    for (size_t i = 0; i < 6; ++i)
        if (a.armor[i].instance) {
            const auto& armor = a.armor[i];
            w.Word(408);
            w.Var(armor.instance);
            w.Var(s.armor[i]);
            w.Byte(static_cast<uint8_t>(remote ? armor.flags & 0xfe : armor.flags));
            w.Var(0);
            for (uint16_t color : s.colors[i])
                w.Word(color);
            w.Var(0);
            w.Word(123);
            w.Var(a.agent);
            w.Var(armor.instance);
            w.Byte(armor.slot);
        }
    for (const auto& weapon : a.weapons)
        if (weapon.instance) {
            w.Word(419);
            w.Var(weapon.instance);
            w.Var(weapon.item);
            w.Byte(static_cast<uint8_t>(remote ? weapon.flags & 0xfe : weapon.flags));
            w.Var(weapon.appearance);
            w.Var(weapon.upgrade);
            w.Byte(weapon.reportState);
            w.Var(weapon.reportValue);
            w.Word(123);
            w.Var(a.agent);
            w.Var(weapon.instance);
            w.Byte(weapon.slot);
        }
    if (a.weaponSetPresent) {
        w.Word(129);
        w.Var(a.agent);
        w.Byte(a.weaponSet);
    }
    if (a.professionStatePresent) {
        w.Word(216);
        w.Var(a.agent);
        w.Var(a.professionState);
    }
    w.Word(119);
    w.Var(a.agent);
    w.Word(187);
    w.Var(world.player);
    w.Var(0);
    w.Word(234);
    w.Var(world.player);
    w.Word(236);
    w.Var(world.player);
    w.Word(61);
    w.Var(a.agent);
    w.Var(0);
    w.Word(67);
    w.Var(a.agent);
    if (!remote) {
        w.Word(183);
        w.Var(world.player);
        w.Var(a.agent);
        for (uint32_t skill : a.learnedSkills) {
            w.Word(264);
            w.Var(skill);
            w.Byte(0);
        }
        for (uint32_t trait : a.automaticTraits) {
            w.Word(292);
            w.Var(trait);
        }
    }
    return w.Finish(out);
}
bool ActivateWorldActor(const MapEntry& world, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!agent || !sequence || !ValidMapEntry(world))
        return false;
    Writer w;
    w.Word(19);
    w.Word(agent);
    w.Var(sequence);
    w.Byte(0x40);
    w.Byte(0);
    w.Byte(6);
    for (float v : world.position)
        w.Float(v);
    w.Float(0);
    w.Word(0);
    w.Word(0);
    return w.Finish(out);
}
bool AnnouncePeer(const MapEntry& world, const RosterEntry& character, const WorldActor& actor, Buffer& out) {
    if (!world.player || world.player >= 255 || actor.agent != 256u + world.player || actor.sequence == UINT32_MAX)
        return false;
    Buffer result, activation;
    if (!EncodeWorldActor(world, character, actor, true, result) ||
        !ActivateWorldActor(world, actor.agent, actor.sequence + 1, activation) || !result.Append(activation.View()))
        return false;
    out = std::move(result);
    return true;
}
bool RemovePeer(uint8_t player, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!player || player == 255 || agent != 256u + player || !sequence)
        return false;
    Buffer result;
    if (!result.U16(16) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x80) || !result.U16(180) ||
        !result.VarU32(player))
        return false;
    out = std::move(result);
    return true;
}
} // namespace july14812
} // namespace glide
