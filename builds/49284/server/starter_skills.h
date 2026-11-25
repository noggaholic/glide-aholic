#pragma once
#include "starter_skill_data.h"
#include "weapon_permissions.h"
#include "world_loadout.h"
#include "../../14812/server/motion.h"
#include "glide/buffer.h"
#include <vector>
#include <algorithm>

namespace glide {
namespace hot49284 {
inline const StarterDefinition* StarterDefinitionFor(uint32_t skill) {
    for (const auto& value : StarterSkillsData().definitions)
        if (value.skill == skill)
            return &value;
    return nullptr;
}
inline const StarterAnimation* StarterAnimationFor(uint32_t skill, bool underwater) {
    for (const auto& value : StarterSkillsData().animations)
        if (value.skill == skill && value.underwater == underwater)
            return &value;
    return nullptr;
}
inline bool StarterAnimationless(uint32_t skill) {
    for (uint32_t id : StarterSkillsData().animationless)
        if (id == skill)
            return true;
    return false;
}
inline bool StarterTrait(unsigned profession, uint32_t id) {
    if (!id)
        return true;
    if (profession < 1 || profession > 9)
        return false;
    const auto& row = kWeaponPermissions[profession - 1];
    for (unsigned i = 0; i < row.count; ++i)
        if (row.traits[i] == id)
            return true;
    return false;
}
inline unsigned StarterWeaponKind(uint32_t item) {
    switch (item) {
    case 33349:
        return 7;
    case 33355:
        return 0;
    case 33350:
        return 8;
    case 33344:
        return 2;
    case 33345:
        return 5;
    case 33343:
        return 4;
    case 33360:
        return 19;
    case 33361:
        return 20;
    case 33359:
        return 21;
    default:
        return 24;
    }
}
inline bool StarterKindMatches(unsigned expected, unsigned actual) {
    return expected == 24 || (expected == 25 ? (actual == 24 || actual == 25) : actual == expected);
}
inline uint32_t EquippedStarterWeapon(const Hot49284Loadout& recipe, uint8_t equipped, unsigned slot) {
    for (unsigned i = 0; i < 6; ++i)
        if ((equipped & (1u << i)) && recipe.weapons[i].slot == slot)
            return recipe.weapons[i].item;
    return 0;
}
inline const StarterChoice* SelectStarterSkill(const Hot49284Loadout& recipe, uint8_t equipped, unsigned level,
                                               unsigned weaponSlot, unsigned barSlot, unsigned professionState,
                                               uint32_t predecessor, unsigned chainCount, uint32_t activeEffect = 0) {
    const uint32_t weapon = EquippedStarterWeapon(recipe, equipped, weaponSlot);
    if (!weapon)
        return nullptr;
    const auto mainKind = StarterWeaponKind(weapon);
    const auto offKind = weaponSlot == 24 ? 24 : StarterWeaponKind(EquippedStarterWeapon(recipe, equipped, 30));
    for (const auto& entry : StarterSkillsData().choices) {
        if (entry.profession != recipe.profession || entry.weapon != weapon || entry.weaponSlot != weaponSlot ||
            entry.slot != barSlot || level < entry.minimumLevel ||
            !EquippedStarterWeapon(recipe, equipped, entry.requiredEquipmentSlot))
            continue;
        // Effects are supplied only by the validated controller state. Keep the
        // unresolved native mode predicate distinct from water/sex guesses.
        if ((entry.requiredEffect && entry.requiredEffect != activeEffect) || entry.modePredicate != 2 ||
            !StarterTrait(recipe.profession, entry.requiredTrait) ||
            (entry.professionState != 17 && entry.professionState != professionState) ||
            (entry.predecessor && (entry.predecessor != predecessor || chainCount < entry.chainCount)) ||
            !StarterKindMatches(entry.mainHandKind, mainKind) || !StarterKindMatches(entry.offHandKind, offKind))
            continue;
        return &entry;
    }
    return nullptr;
}
inline bool StarterSkillGrants(const Hot49284Loadout& recipe, unsigned level, uint32_t player, Buffer& out) {
    if (!player || player >= 255 || !level || level > 80 || recipe.profession < 1 || recipe.profession > 9)
        return false;
    std::vector<uint32_t> granted;
    Buffer result;
    for (const auto& choice : StarterSkillsData().choices) {
        if (choice.profession != recipe.profession || level < choice.minimumLevel ||
            EquippedStarterWeapon(recipe, 63, choice.weaponSlot) != choice.weapon)
            continue;
        const auto* definition = StarterDefinitionFor(choice.skill);
        if (!definition || definition->kind || definition->recharge < 0 ||
            std::find(granted.begin(), granted.end(), choice.skill) != granted.end())
            continue;
        // S416: SkillDef DataId, then Player identity (not progress).
        // Learning only grants availability; palette predicates remain active.
        if (!result.U16(416) || !result.VarU32(choice.skill) || !result.VarU32(player))
            return false;
        granted.push_back(choice.skill);
    }
    // Own support palette4564, independent of the equipped weapon.
    if (recipe.profession == 5)
        for (uint32_t skill : {13027u, 13050u})
            if (!result.U16(416) || !result.VarU32(skill) || !result.VarU32(player))
                return false;
    if (recipe.profession == 3 && (!result.U16(416) || !result.VarU32(5834) || !result.VarU32(player)))
        return false;
    if (recipe.profession == 6 && (!result.U16(416) || !result.VarU32(5503) || !result.VarU32(player)))
        return false;
    // The existing palettes also need learned bits for the hostile actions.
    // Native HoT otherwise leaves slot2 empty despite a working server handler.
    if (recipe.profession == 9)
        for (uint32_t skill :
             {26557u, 26956u, 27372u, 28219u, 27975u, 28287u, 26693u, 27322u, 28516u, 28231u, 27107u, 28382u, 29209u,
              27917u, 27220u, 27228u, 28379u, 26644u, 27014u, 27760u, 28075u, 26679u, 26937u, 28406u, 27505u})
            if (!result.U16(416) || !result.VarU32(skill) || !result.VarU32(player))
                return false;
    if (granted.empty())
        return false;
    out = std::move(result);
    return true;
}
inline bool StarterThiefHealingPalette(uint32_t player, Buffer& out) {
    // Own land268 and aquatic133; initial medium assignment is explicit policy.
    return player && player < 255 && out.U16(421) && out.VarU32(268) && out.U8(0) && out.U8(0) && out.VarU32(player) &&
           out.U16(421) && out.VarU32(133) && out.U8(0) && out.U8(1) && out.VarU32(player);
}
inline bool StarterEngineerHealingPalette(uint32_t player, Buffer& out) {
    // Explicit compatible aquatic assignment; own palette276/profession3.
    return player && player < 255 && out.U16(421) && out.VarU32(276) && out.U8(0) && out.U8(1) && out.VarU32(player);
}
inline bool StarterElementalistHealingPalette(uint32_t player, Buffer& out) {
    // Local initial support assignment in both media, using own palette116.
    if (!player || player >= 255)
        return false;
    for (uint8_t medium = 0; medium < 2; ++medium)
        if (!out.U16(421) || !out.VarU32(116) || !out.U8(0) || !out.U8(medium) || !out.VarU32(player))
            return false;
    return true;
}
inline bool StarterHammersPalette(uint32_t player, Buffer& out) {
    if (!player || player >= 255)
        return false;
    Buffer result;
    // S421 fills the player's support palette arrays, independently of learned skills. Palette4564
    // selects26557/26956 through state13/effect27273, or 29209 in state12 and27917 in state14. Only those rows
    // are currently learned. Both arrays persist while the native legend state changes; reconnect rebuilds them.
    // No other support skill is granted by this assignment.
    for (uint8_t environment = 0; environment < 2; ++environment)
        if (!result.U16(421) || !result.VarU32(4564) || !result.U8(3) || !result.U8(environment) ||
            !result.VarU32(player))
            return false;
    out = std::move(result);
    return true;
}
inline bool StarterDwarfHealingPalette(uint32_t player, Buffer& out) {
    if (!player || player >= 255)
        return false;
    Buffer result;
    // 4572 selects27372/state13,28219/state14 or26937/state12.
    // Own dwarf/demon-healing records under data/49284.
    for (uint8_t environment = 0; environment < 2; ++environment)
        if (!result.U16(421) || !result.VarU32(4572) || !result.U8(0) || !result.U8(environment) ||
            !result.VarU32(player))
            return false;
    out = std::move(result);
    return true;
}
inline bool StarterGreatDwarfPalette(uint32_t player, Buffer& out) {
    if (!player || player >= 255)
        return false;
    Buffer result;
    // 4554 selects27975/state13,28406/state12 or28287/26693/state14+effect28001.
    // Elite slot4, both environments; own great-dwarf/demon-elite records.
    for (uint8_t environment = 0; environment < 2; ++environment)
        if (!result.U16(421) || !result.VarU32(4554) || !result.U8(4) || !result.U8(environment) ||
            !result.VarU32(player))
            return false;
    out = std::move(result);
    return true;
}
inline bool StarterPainAbsorptionPalette(uint32_t player, Buffer& out) {
    if (!player || player >= 255)
        return false;
    Buffer result;
    // 4614/slot1 selects27322/demon14,28516/dwarf13 or28231/assassin12.
    // Remaining rows stay ungranted; this assignment already covers both media.
    for (uint8_t environment = 0; environment < 2; ++environment)
        if (!result.U16(421) || !result.VarU32(4614) || !result.U8(1) || !result.U8(environment) ||
            !result.VarU32(player))
            return false;
    out = std::move(result);
    return true;
}
inline bool StarterAssassinTogglePalette(uint32_t player, Buffer& out) {
    if (!player || player >= 255)
        return false;
    // 4651/slot2:27107/28382 in state12,26679 in13 and27505 in14. HoT's existing aquatic legend policy still
    // excludes12.
    for (uint8_t environment = 0; environment < 2; ++environment)
        if (!out.U16(421) || !out.VarU32(4651) || !out.U8(2) || !out.U8(environment) || !out.VarU32(player))
            return false;
    return true;
}
inline bool StarterPlan(const StarterAnimation& animation, std::vector<july14812::SkillStage>& out,
                        uint32_t& duration) {
    if (!animation.count || animation.count > 32 ||
        static_cast<size_t>(animation.first) + animation.count > StarterSkillsData().phases.size())
        return false;
    std::vector<july14812::SkillStage> next;
    uint32_t total = 0;
    for (unsigned i = 0; i < animation.count; ++i) {
        const auto& original = StarterSkillsData().phases[animation.first + i];
        july14812::SkillStage phase;
        phase.index = original.index;
        phase.type = original.type;
        phase.durationMs = original.duration;
        phase.movement = original.movement;
        if (!phase.durationMs || phase.index != i)
            return false;
        if (phase.type) {
            july14812::SkillMovement move;
            if (!july14812::BuildSkillMovement(phase, {{1, 0, 0}}, move, animation.underwater))
                return false;
        }
        total += phase.durationMs;
        next.push_back(phase);
    }
    if (total > 60000)
        return false;
    out = std::move(next);
    duration = total;
    return true;
}
} // namespace hot49284
} // namespace glide
