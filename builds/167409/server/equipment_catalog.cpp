#include "../../204489/server/aquatic_skills.h"
#include "equipment_catalog.h"
#include "profession_state.h"
#include "../../204489/server/weapon_ranges.h"
#include "../../204489/server/aquatic_equipment.h"
#include "inventory_state.h"
#include "glide/files.h"
#include "glide/profile/auth_codec.h"
#include <sstream>
#include <utility>
#include <cmath>

namespace glide {
const AuthCodec& Build204489Auth();
namespace china167409 {
namespace {
unsigned Key(unsigned race, unsigned sex, unsigned profession) {
    return (race * 2 + sex) * 10 + profession;
}
bool Field(const Character& saved, const char* name, unsigned maximum, unsigned& value) {
    const auto at = saved.profileFields.find(name);
    return at != saved.profileFields.end() && ParseUnsigned(at->second, maximum, value);
}
int ColorIndex(size_t slot) {
    // Original CcContext packs the six equipment slots 2..7 in slot order:
    // chest, legs, hands, head, feet, shoulders.
    return slot >= 2 && slot <= 7 ? static_cast<int>(slot - 2) : -1;
}
bool Colors(const Character& saved, std::array<uint16_t, 24>& out) {
    std::array<uint16_t, 24> next{};
    const auto at = saved.profileFields.find("creation_colors");
    if (at != saved.profileFields.end()) {
        std::vector<uint8_t> bytes;
        if (!DecodeHex(at->second, 96, bytes) || bytes.size() != 96)
            return false;
        Reader reader(ByteView(bytes.data(), bytes.size()));
        for (auto& color : next) {
            uint32_t value = 0;
            if (!reader.U32(value) || value > 65535)
                return false;
            color = static_cast<uint16_t>(value);
        }
    }
    out = next;
    return true;
}
bool BindingOwner(const Guid& owner, bool bound, Buffer& out) {
    // Both native reports carry an optional 16-byte character GUID. The flag
    // without that owner makes the native UI treat the item as bound to someone else.
    return out.U8(bound ? uint8_t(1) : uint8_t(0)) && (!bound || out.Append(ByteView(owner.bytes, 16)));
}
bool ItemReport(uint32_t instance, uint32_t definition, uint8_t type, const std::array<uint16_t, 4>& dyes,
                const Guid& owner, bool bound, bool ownerView, Buffer& out) {
    // Native bit 5 means soulbound; bit 0 confirms ownership in this viewer's
    // session. Another player receives the binding and GUID without bit 0.
    const uint8_t flags = bound ? uint8_t(ownerView ? 0x21 : 0x20) : uint8_t(0);
    if (type == 24) {
        // S941 calls the original weapon report, including its optional skin,
        // upgrades and state. A new unmodified starter weapon has none of these.
        if (!out.U16(941) || !out.VarU32(instance) || !out.VarU32(definition) || !out.U8(flags))
            return false;
        for (unsigned field = 0; field < 5; ++field)
            if (!out.VarU32(0))
                return false;
        return out.U8(0) && out.VarU32(0) && out.U8(0) && out.U64(0) && out.U8(0) && BindingOwner(owner, bound, out);
    }
    // Original S930 dispatches to ItCliArmor::Report; S931 only updates base identity.
    if (!out.U16(930) || !out.VarU32(instance) || !out.VarU32(definition) || !out.U8(flags) || !out.VarU32(0) ||
        !out.VarU32(0))
        return false;
    for (auto dye : dyes)
        if (!out.U16(dye))
            return false;
    // The two original color accessors use these arrays. An initial,
    // unmodified garment carries its saved dyes in both.
    for (auto dye : dyes)
        if (!out.U16(dye))
            return false;
    return out.VarU32(0) && out.VarU32(0) && out.VarU32(0) && out.U8(0) && BindingOwner(owner, bound, out);
}
} // namespace
bool EquipmentCatalog::Load(const std::wstring& dataRoot, std::string& error) {
    Buffer input;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-equipment.txt", 1024 * 1024, input)) {
        error = "Cannot read original China initial equipment";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(input.Data()), input.Size()));
    std::map<unsigned, InitialEquipment> next;
    std::string line;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned race = 0, sex = 0, profession = 0, level = 0;
        if (!(row >> race >> sex >> profession >> level) || race > 4 || sex > 1 || !profession || profession > 9 ||
            level != 1) {
            error = "Invalid China equipment recipe identity";
            return false;
        }
        InitialEquipment equipment;
        for (auto& id : equipment.definitions)
            if (!(row >> id) || id >= 0x400000) {
                error = "Invalid original China item DataId";
                return false;
            }
        for (size_t slot = 0; slot < equipment.types.size(); ++slot) {
            unsigned type = 0;
            if (!(row >> type) || (type != 0 && type != 24) || (!equipment.definitions[slot] && type)) {
                error = "Invalid original China starter item type";
                return false;
            }
            equipment.types[slot] = static_cast<uint8_t>(type);
        }
        std::string extra;
        if (row >> extra || !next.emplace(Key(race, sex, profession), equipment).second) {
            error = "Duplicate or trailing China equipment data";
            return false;
        }
    }
    if (next.size() != 90) {
        error = "China equipment must cover five races, two sexes and nine professions";
        return false;
    }
    Buffer biographyInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-biography-equipment.txt", 1024 * 1024,
                       biographyInput)) {
        error = "Cannot read original China biography equipment";
        return false;
    }
    std::istringstream biographyDocument(
        std::string(reinterpret_cast<const char*>(biographyInput.Data()), biographyInput.Size()));
    std::map<unsigned, std::vector<BiographyChoice>> biography;
    size_t rows = 0;
    while (std::getline(biographyDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned race = 0, profession = 0, allowed = 0, type = 0;
        BiographyChoice choice;
        if (!(row >> race >> profession >> choice.response >> choice.ordinal >> allowed >> choice.slot >>
              choice.definition >> type) ||
            race > 4 || !profession || profession > 9 || choice.response > 255 || allowed > 1 || choice.slot > 68 ||
            choice.definition >= 0x400000 || (type != 0 && type != 2) ||
            ((!choice.definition) != (choice.slot == 68)) || (!choice.definition && type) ||
            (type == 2 && choice.slot != 1) ||
            (choice.definition && type == 0 && choice.slot != 0 && choice.slot != 2 && choice.slot != 3 &&
             choice.slot != 4 && choice.slot != 5 && choice.slot != 6 && choice.slot != 7)) {
            error = "Invalid original China biography equipment";
            return false;
        }
        auto& group = biography[Key(race, 0, profession)];
        std::string extra;
        if (row >> extra || choice.ordinal != group.size()) {
            error = "Invalid China biography choice order";
            return false;
        }
        for (const auto& previous : group)
            if (previous.response == choice.response) {
                error = "Duplicate China biography response";
                return false;
            }
        choice.allowed = allowed != 0;
        choice.type = static_cast<uint8_t>(type);
        group.push_back(choice);
        ++rows;
    }
    if (rows != 175 || biography.size() != 45) {
        error = "Incomplete original China biography equipment";
        return false;
    }
    for (unsigned race = 0; race < 5; ++race)
        for (unsigned profession = 1; profession <= 9; ++profession) {
            const auto group = biography.find(Key(race, 0, profession));
            const size_t count = profession == 4 ? 10 : profession == 6 ? 4 : 3;
            if (group == biography.end() || group->second.size() != count) {
                error = "Incomplete China profession choices";
                return false;
            }
        }
    Buffer appearanceInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-item-appearance.txt", 256 * 1024, appearanceInput)) {
        error = "Cannot read original China item appearance";
        return false;
    }
    std::istringstream appearanceDocument(
        std::string(reinterpret_cast<const char*>(appearanceInput.Data()), appearanceInput.Size()));
    std::map<uint32_t, ItemAppearance> appearances;
    while (std::getline(appearanceDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned item = 0, skin = 0, type = 0;
        std::string extra;
        if (!(row >> item >> skin >> type) || !item || item >= 0x400000 || !skin || skin >= 0x400000 ||
            (type != 0 && type != 2 && type != 9 && type != 24) || row >> extra) {
            error = "Invalid original China item appearance";
            return false;
        }
        ItemAppearance value;
        value.skin = skin;
        value.type = static_cast<uint8_t>(type);
        if (!appearances.emplace(item, value).second) {
            error = "Duplicate China item appearance";
            return false;
        }
    }
    if (appearances.size() != 2318) {
        error = "Incomplete original China item appearance";
        return false;
    }
    for (const auto& recipe : next)
        for (size_t slot = 0; slot < recipe.second.definitions.size(); ++slot) {
            const uint32_t id = recipe.second.definitions[slot];
            if (!id)
                continue;
            const auto found = appearances.find(id);
            if (found == appearances.end() || found->second.type != recipe.second.types[slot]) {
                error = "China recipe appearance mismatch";
                return false;
            }
        }
    for (const auto& group : biography)
        for (const auto& choice : group.second)
            if (choice.definition) {
                const auto found = appearances.find(choice.definition);
                if (found == appearances.end() || found->second.type != choice.type) {
                    error = "China biography appearance mismatch";
                    return false;
                }
            }
    Buffer bagInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-bags.txt", 4096, bagInput)) {
        error = "Cannot read original China bags";
        return false;
    }
    std::istringstream bagDocument(std::string(reinterpret_cast<const char*>(bagInput.Data()), bagInput.Size()));
    std::map<uint32_t, unsigned> bags;
    InitialBagDefinition initialBag;
    while (std::getline(bagDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t dataId = 0;
        unsigned capacity = 0, initial = 0;
        std::string extra;
        if (!(row >> dataId >> capacity >> initial) || !dataId || dataId >= 0x400000 || !capacity || capacity > 480 ||
            initial > 1 || row >> extra || !bags.emplace(dataId, capacity).second || (initial && initialBag.dataId)) {
            error = "Invalid original China bag definition";
            return false;
        }
        if (initial) {
            initialBag.dataId = dataId;
            initialBag.capacity = capacity;
        }
    }
    if (bags.size() != 3 || !initialBag.dataId) {
        error = "Incomplete original China bags";
        return false;
    }
    Buffer weaponInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-proficiencies.txt", 4096, weaponInput)) {
        error = "Cannot read original China weapon proficiencies";
        return false;
    }
    std::istringstream weaponDocument(
        std::string(reinterpret_cast<const char*>(weaponInput.Data()), weaponInput.Size()));
    std::map<unsigned, WeaponProficiency> weaponProficiencies;
    static const unsigned professionBits[] = {0, 9, 14, 7, 12, 13, 8, 10, 11, 15};
    while (std::getline(weaponDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0;
        uint32_t restriction = 0;
        WeaponProficiency proficiency;
        std::string extra;
        if (!(row >> profession >> proficiency.traitId >> proficiency.minimumLevel >> restriction >>
              proficiency.mainHandMask >> proficiency.offHandMask >> proficiency.underwaterMask) ||
            !profession || profession > 9 || !proficiency.traitId || proficiency.traitId >= 0x400000 ||
            proficiency.minimumLevel != 1 || restriction != (uint32_t(1) << professionBits[profession]) ||
            !proficiency.mainHandMask || !proficiency.offHandMask || !proficiency.underwaterMask ||
            proficiency.mainHandMask >= 0x1000000 || proficiency.offHandMask >= 0x1000000 ||
            proficiency.underwaterMask >= 0x1000000 || row >> extra ||
            !weaponProficiencies.emplace(profession, proficiency).second) {
            error = "Invalid original China weapon proficiency";
            return false;
        }
    }
    if (weaponProficiencies.size() != 9) {
        error = "Incomplete original China weapon proficiencies";
        return false;
    }
    Buffer handInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-types.txt", 4096, handInput)) {
        error = "Cannot read original China weapon hand requirements";
        return false;
    }
    std::istringstream handDocument(std::string(reinterpret_cast<const char*>(handInput.Data()), handInput.Size()));
    std::map<uint32_t, bool> twoHandedWeapons;
    while (std::getline(handDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t definition = 0;
        unsigned type = 0, twoHanded = 0;
        std::string extra;
        if (!(row >> definition >> type >> twoHanded) || !definition || definition >= 0x400000 || type > 25 ||
            twoHanded > 1 || row >> extra || !twoHandedWeapons.emplace(definition, twoHanded != 0).second) {
            error = "Invalid original China weapon hand requirement";
            return false;
        }
        const auto appearance = appearances.find(definition);
        if (appearance == appearances.end() || appearance->second.type != 24) {
            error = "Original China weapon hand requirement is not a weapon";
            return false;
        }
    }
    for (const auto& recipe : next)
        for (size_t slot = 29; slot <= 32; ++slot)
            if (recipe.second.definitions[slot] &&
                twoHandedWeapons.find(recipe.second.definitions[slot]) == twoHandedWeapons.end()) {
                error = "Missing original China weapon hand requirement";
                return false;
            }
    Buffer skillInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-skills.txt", 16384, skillInput)) {
        error = "Cannot read original China initial weapon skills";
        return false;
    }
    std::istringstream skillDocument(std::string(reinterpret_cast<const char*>(skillInput.Data()), skillInput.Size()));
    std::map<unsigned, std::vector<InitialWeaponSkill>> weaponSkills;
    while (std::getline(skillDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0;
        InitialWeaponSkill skill;
        std::string extra;
        if (!(row >> profession >> skill.weapon >> skill.slotType >> skill.skill >> skill.minimumLevel) ||
            !profession || profession > 9 || !skill.weapon || skill.weapon >= 0x400000 || skill.slotType < 3 ||
            skill.slotType > 7 || !skill.skill || skill.skill >= 0x400000 || skill.minimumLevel != 1 || row >> extra) {
            error = "Invalid original China initial weapon skill";
            return false;
        }
        const auto hands = twoHandedWeapons.find(skill.weapon);
        if (hands == twoHandedWeapons.end()) {
            error = "Missing original China skill weapon hand requirement";
            return false;
        }
        skill.equipmentSlot = skill.slotType >= 6 && !hands->second ? 30u : 29u;
        auto& entries = weaponSkills[profession];
        if (entries.size() >= 64) {
            error = "Too many original China initial weapon skills";
            return false;
        }
        for (const auto& prior : entries)
            if (prior.skill == skill.skill || prior.weapon != skill.weapon) {
                error = "Conflicting original China initial weapon skills";
                return false;
            }
        for (unsigned race = 0; race < 5; ++race)
            for (unsigned sex = 0; sex < 2; ++sex) {
                const auto recipe = next.find(Key(race, sex, profession));
                if (recipe == next.end() || recipe->second.definitions[29] != skill.weapon) {
                    error = "Original China initial weapon skill does not match equipment";
                    return false;
                }
            }
        entries.push_back(skill);
    }
    if (weaponSkills.size() != 9) {
        error = "Incomplete original China initial weapon skills";
        return false;
    }
    Buffer actionInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-skill-actions.txt", 16384, actionInput)) {
        error = "Cannot read original China weapon skill timing";
        return false;
    }
    std::istringstream actionDocument(
        std::string(reinterpret_cast<const char*>(actionInput.Data()), actionInput.Size()));
    std::map<uint32_t, InitialWeaponSkill> timings;
    while (std::getline(actionDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        InitialWeaponSkill timing;
        unsigned variable = 0;
        std::string extra;
        if (!(row >> timing.skill >> timing.activationMs >> timing.animationMs >> timing.recoveryMs >> variable) ||
            !timing.skill || timing.skill >= 0x400000 || timing.activationMs > 65535 || timing.animationMs > 65535 ||
            variable > 1 || row >> extra ||
            (timing.animationMs ? (!timing.activationMs || timing.recoveryMs >= timing.animationMs)
                                : (timing.activationMs || timing.recoveryMs || variable))) {
            error = "Invalid original China weapon skill timing";
            return false;
        }
        timing.variableStages = variable != 0;
        if (!timings.emplace(timing.skill, timing).second) {
            error = "Duplicate original China weapon skill timing";
            return false;
        }
    }
    size_t matchedTimings = 0;
    for (auto& profession : weaponSkills)
        for (auto& skill : profession.second) {
            const auto found = timings.find(skill.skill);
            if (found == timings.end()) {
                error = "Missing original China weapon skill timing";
                return false;
            }
            skill.activationMs = found->second.activationMs;
            skill.animationMs = found->second.animationMs;
            skill.recoveryMs = found->second.recoveryMs;
            skill.variableStages = found->second.variableStages;
            ++matchedTimings;
        }
    if (matchedTimings != timings.size()) {
        error = "Unmatched original China weapon skill timing";
        return false;
    }
    std::map<uint32_t, modern::SkillPhases> phases;
    if (!modern::LoadSkillPhases(dataRoot + L"/167409/characters/initial-weapon-skill-phases.txt", phases)) {
        error = "Cannot read original China weapon skill phases";
        return false;
    }
    size_t matchedPhases = 0;
    for (auto& profession : weaponSkills)
        for (auto& skill : profession.second) {
            const auto found = phases.find(skill.skill);
            if (!skill.animationMs) {
                if (found != phases.end()) {
                    error = "Unexpected China instant skill phases";
                    return false;
                }
                continue;
            }
            if (found == phases.end()) {
                error = "Missing original China weapon skill phases";
                return false;
            }
            unsigned duration = 0;
            bool moving = false;
            for (const auto& phase : found->second) {
                duration += phase.durationMs;
                moving = moving || phase.type != 0;
            }
            if (duration != skill.animationMs || moving != skill.variableStages ||
                (moving && skill.activationMs + skill.recoveryMs > duration)) {
                error = "Inconsistent original China weapon skill phases";
                return false;
            }
            skill.phases = found->second;
            ++matchedPhases;
        }
    if (matchedPhases != phases.size()) {
        error = "Unmatched original China weapon skill phases";
        return false;
    }
    Buffer rechargeInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-skill-recharges.txt", 16384, rechargeInput)) {
        error = "Cannot read original China weapon skill recharge";
        return false;
    }
    std::istringstream rechargeDocument(
        std::string(reinterpret_cast<const char*>(rechargeInput.Data()), rechargeInput.Size()));
    std::map<uint32_t, unsigned> recharges;
    while (std::getline(rechargeDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t skill = 0;
        unsigned recharge = 0;
        std::string extra;
        if (!(row >> skill >> recharge) || !skill || skill >= 0x400000 || recharge > 3600000 || row >> extra) {
            error = "Invalid original China weapon skill recharge";
            return false;
        }
        if (!recharges.emplace(skill, recharge).second) {
            error = "Duplicate original China weapon skill recharge";
            return false;
        }
    }
    size_t matchedRecharges = 0;
    for (auto& profession : weaponSkills)
        for (auto& skill : profession.second) {
            const auto found = recharges.find(skill.skill);
            if (found == recharges.end()) {
                error = "Missing original China weapon skill recharge";
                return false;
            }
            skill.rechargeMs = found->second;
            ++matchedRecharges;
        }
    if (matchedRecharges != recharges.size()) {
        error = "Unmatched original China weapon skill recharge";
        return false;
    }
    Buffer targetingInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-weapon-skill-targeting.txt", 4096, targetingInput)) {
        error = "Cannot read original China weapon skill targeting";
        return false;
    }
    std::istringstream targetingDocument(
        std::string(reinterpret_cast<const char*>(targetingInput.Data()), targetingInput.Size()));
    std::map<uint32_t, bool> targeting;
    while (std::getline(targetingDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t skill = 0;
        unsigned ground = 0;
        std::string extra;
        if (!(row >> skill >> ground) || !skill || skill >= 0x400000 || ground > 1 || row >> extra ||
            !targeting.emplace(skill, ground != 0).second) {
            error = "Invalid original China weapon skill targeting";
            return false;
        }
    }
    size_t matchedTargeting = 0;
    for (auto& profession : weaponSkills)
        for (auto& skill : profession.second) {
            const auto found = targeting.find(skill.skill);
            if (found == targeting.end()) {
                error = "Missing original China weapon skill targeting";
                return false;
            }
            skill.groundTargeted = found->second;
            ++matchedTargeting;
        }
    if (matchedTargeting != targeting.size()) {
        error = "Unmatched original China weapon skill targeting";
        return false;
    }
    Buffer rangeInput;
    if (!ReadFileBytes(dataRoot + L"\\167409\\characters\\initial-ground-skill-ranges.txt", 4096, rangeInput)) {
        error = "Cannot read original China ground skill ranges";
        return false;
    }
    std::istringstream rangeDocument(std::string(reinterpret_cast<const char*>(rangeInput.Data()), rangeInput.Size()));
    std::map<uint32_t, std::pair<float, float>> ranges;
    while (std::getline(rangeDocument, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t skill = 0;
        float minimum = 0, maximum = 0;
        std::string extra;
        if (!(row >> skill >> minimum >> maximum) || !skill || skill >= 0x400000 || row >> extra ||
            !std::isfinite(minimum) || !std::isfinite(maximum * maximum) || minimum < 0 || maximum <= minimum ||
            !ranges.emplace(skill, std::make_pair(minimum, maximum)).second) {
            error = "Invalid original China ground skill range";
            return false;
        }
    }
    size_t matchedRanges = 0;
    for (auto& profession : weaponSkills)
        for (auto& skill : profession.second)
            if (skill.groundTargeted) {
                const auto found = ranges.find(skill.skill);
                if (found == ranges.end()) {
                    error = "Missing original China ground skill range";
                    return false;
                }
                skill.minimumGroundRange = found->second.first;
                skill.maximumGroundRange = found->second.second;
                ++matchedRanges;
            }
    if (matchedRanges != ranges.size()) {
        error = "Unmatched original China ground skill range";
        return false;
    }
    if (!modern::LoadWeaponTargetRanges(dataRoot + L"/167409/characters/initial-weapon-target-ranges.txt", weaponSkills,
                                        error))
        return false;
    std::array<modern::AquaticEquipment, 10> aquatic;
    if (!modern::LoadAquaticEquipment(dataRoot + L"/167409/characters/aquatic-equipment.txt", aquatic, error))
        return false;
    std::map<unsigned, std::vector<InitialWeaponSkill>> aquaticSkills;
    if (!modern::LoadAquaticSkills(dataRoot + L"/167409/characters", aquatic, aquaticSkills, error))
        return false;
    std::map<unsigned, bool> added;
    for (auto& row : next) {
        const auto profession = row.first % 10;
        const auto& item = aquatic[profession];
        if (!item.item)
            continue;
        const auto permission = weaponProficiencies.find(profession);
        const auto skin = appearances.find(item.item);
        if (permission == weaponProficiencies.end() || !(permission->second.underwaterMask & (1u << item.kind)) ||
            skin == appearances.end() || skin->second.skin != item.skin || skin->second.type != 24 ||
            (row.second.definitions[24] && row.second.definitions[24] != item.item)) {
            error = "Aquatic equipment does not match original China permissions or appearance";
            return false;
        }
        added[row.first] = row.second.definitions[24] == 0;
        row.second.definitions[24] = item.item;
        row.second.types[24] = 24;
    }
    modern::SupportPalettes supportPalettes;
    modern::SupportUnlocks supportUnlocks;
    if (!modern::LoadSupportLoadout(dataRoot, 167409, supportPalettes, supportUnlocks, error))
        return false;
    supportPalettes_ = std::move(supportPalettes);
    supportUnlocks_ = std::move(supportUnlocks);
    aquaticSkills_ = std::move(aquaticSkills);
    aquaticAdded_ = std::move(added);
    recipes_ = std::move(next);
    biography_ = std::move(biography);
    appearances_ = std::move(appearances);
    initialBag_ = initialBag;
    weaponProficiencies_ = std::move(weaponProficiencies);
    weaponSkills_ = std::move(weaponSkills);
    error.clear();
    return true;
}
bool EquipmentCatalog::SupportSkillPackets(const Character& saved, Buffer& out) const {
    unsigned profession = 0;
    return Field(saved, "profession", 9, profession) &&
           modern::SupportLoadoutPackets(167409, supportPalettes_, supportUnlocks_, profession, saved.level, out);
}
bool EquipmentCatalog::ResolveInitialWeaponSkills(const Character& saved,
                                                  const std::vector<InitialWeaponSkill>*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = weaponSkills_.find(profession);
    if (found == weaponSkills_.end() || found->second.empty())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::ResolveAquaticWeaponSkills(const Character& saved,
                                                  const std::vector<InitialWeaponSkill>*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = aquaticSkills_.find(profession);
    if (found == aquaticSkills_.end())
        return false;
    out = &found->second;
    return true;
}
bool InitialWeaponSkillPackets(const EquipmentCatalog& catalog, const Character& saved, Buffer& out) {
    const std::vector<InitialWeaponSkill>*skills = nullptr, *aquatic = nullptr;
    if (!catalog.ResolveInitialWeaponSkills(saved, skills) || !catalog.ResolveAquaticWeaponSkills(saved, aquatic))
        return false;
    Buffer next;
    for (const auto* group : {skills, aquatic})
        for (const auto& skill : *group)
            if (saved.level >= skill.minimumLevel) {
                if (!next.U16(585) || !next.VarU32(skill.skill) || !next.VarU32(1))
                    return false;
            }
    uint8_t attunement = 0;
    if (!SavedAttunement(saved, attunement))
        return false;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state)) {
                if (!next.U16(585) || !next.VarU32(5491 + state) || !next.VarU32(1))
                    return false;
            }
    const auto profession = saved.profileFields.find("profession");
    if (profession != saved.profileFields.end() && profession->second == "9")
        for (uint32_t selector : {28085u, 28134u, 28419u, 28494u}) {
            if (!next.U16(585) || !next.VarU32(selector) || !next.VarU32(1))
                return false;
        }
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::ResolveWeaponProficiency(const Character& saved, const WeaponProficiency*& out) const {
    unsigned profession = 0;
    if (!Field(saved, "profession", 9, profession) || !profession || !saved.level || saved.level > 80)
        return false;
    const auto found = weaponProficiencies_.find(profession);
    if (found == weaponProficiencies_.end() || saved.level < found->second.minimumLevel)
        return false;
    out = &found->second;
    return true;
}
bool InitialWeaponProficiencyPacket(const EquipmentCatalog& catalog, const Character& saved, Buffer& out) {
    const WeaponProficiency* proficiency = nullptr;
    if (!catalog.ResolveWeaponProficiency(saved, proficiency))
        return false;
    Buffer next;
    // S657: player, active traits, unlocked selectable traits, five legacy line progress values.
    // The original client derives all three weapon masks from the native trait definition.
    uint8_t attunement = 0;
    if (!SavedAttunement(saved, attunement))
        return false;
    uint8_t count = 1;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state))
                ++count;
    if (!next.U16(657) || !next.VarU32(1) || !next.U8(count) || !next.VarU32(proficiency->traitId))
        return false;
    if (attunement)
        for (unsigned state = 1; state <= 4; ++state)
            if (saved.level >= AttunementLevel(state)) {
                if (!next.VarU32(AttunementTrait(state)))
                    return false;
            }
    if (!next.U8(0) || !next.U8(5))
        return false;
    for (unsigned line = 0; line < 5; ++line)
        if (!next.U8(0))
            return false;
    out = std::move(next);
    return true;
}
bool InitialBagPackets(const EquipmentCatalog& catalog, uint8_t player, uint16_t agent, Buffer& out) {
    const auto& definition = catalog.InitialBag();
    if (!player || player == 255 || !agent || !definition.dataId || !definition.capacity || definition.capacity > 480)
        return false;
    // Share the existing per-player item namespace; 127 is outside all 68 equipment slots.
    const uint32_t instance = static_cast<uint32_t>(player) * 128 + 127;
    Buffer next;
    // Original S940 reports a Bag definition. Its base report leaves the reserved byte unused.
    if (!next.U16(940) || !next.VarU32(instance) || !next.VarU32(definition.dataId) || !next.U8(0) || !next.U8(0) ||
        !next.U8(0) || !next.U16(286) || !next.VarU32(agent) || !next.U8(1) || !next.U16(285) || !next.VarU32(agent) ||
        !next.VarU32(0) || !next.VarU32(instance))
        return false;
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::Resolve(const Character& saved, const InitialEquipment*& out) const {
    unsigned race = 0, sex = 0, profession = 0, requestedLevel = 1;
    if (!Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 9, profession) ||
        !profession)
        return false;
    const auto level = saved.profileFields.find("requested_level");
    if (level != saved.profileFields.end() && (!ParseUnsigned(level->second, 80, requestedLevel) || !requestedLevel))
        return false;
    // Native race/sex roots have levels 0 and 1; their lookup decrements from
    // the requested level until it finds the profession's matching recipe.
    const auto found = recipes_.find(Key(race, sex, profession));
    if (found == recipes_.end())
        return false;
    out = &found->second;
    return true;
}
bool EquipmentCatalog::HasAddedAquaticWeapon(const Character& saved) const {
    unsigned race = 0, sex = 0, profession = 0;
    if (!Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 9, profession))
        return false;
    const auto found = aquaticAdded_.find(Key(race, sex, profession));
    return found != aquaticAdded_.end() && found->second;
}
bool EquipmentCatalog::ResolveInitial(const Character& saved, InitialEquipment& out) const {
    const InitialEquipment* base = nullptr;
    unsigned race = 0, profession = 0;
    if (!Resolve(saved, base) || !Field(saved, "race", 4, race) || !Field(saved, "profession", 9, profession))
        return false;
    const auto group = biography_.find(Key(race, 0, profession));
    if (group == biography_.end() || group->second.empty())
        return false;
    std::vector<uint8_t> answers;
    const auto stored = saved.profileFields.find("biography");
    if (stored != saved.profileFields.end() && !DecodeHex(stored->second, 32, answers))
        return false;
    // Native CcContext searches bytes in saved order; ordinal zero is its fallback.
    const BiographyChoice* selected = &group->second.front();
    bool found = false;
    for (auto answer : answers) {
        for (const auto& choice : group->second)
            if (choice.allowed && choice.response == answer) {
                selected = &choice;
                found = true;
                break;
            }
        if (found)
            break;
    }
    InitialEquipment next = *base;
    if (selected->definition) {
        next.definitions[selected->slot] = selected->definition;
        next.types[selected->slot] = selected->type;
    }
    out = next;
    return true;
}
bool OriginalInventoryItemReport(const EquipmentCatalog& catalog, const Character& saved, unsigned slot,
                                 uint32_t instance, Buffer& out, bool bound, bool ownerView) {
    InitialEquipment equipment;
    std::array<uint16_t, 24> colors{};
    if (slot >= equipment.definitions.size() || !instance || !catalog.ResolveInitial(saved, equipment) ||
        !equipment.definitions[slot] || !Colors(saved, colors))
        return false;
    std::array<uint16_t, 4> dyes{};
    const int group = equipment.types[slot] == 0 ? ColorIndex(slot) : -1;
    if (group >= 0)
        for (size_t channel = 0; channel < dyes.size(); ++channel)
            dyes[channel] = colors[static_cast<size_t>(group) * 4 + channel];
    Buffer next;
    if (!ItemReport(instance, equipment.definitions[slot], equipment.types[slot], dyes, saved.guid, bound, ownerView,
                    next))
        return false;
    out = std::move(next);
    return true;
}
bool InitialEquipmentPackets(const EquipmentCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                             Buffer& out, bool includeBagItems) {
    InitialEquipment equipment;
    if (!player || player == 255 || !agent || !catalog.ResolveInitial(saved, equipment))
        return false;
    std::array<uint16_t, 24> colors{};
    if (!Colors(saved, colors))
        return false;
    InventoryState state;
    std::string error;
    if (!LoadInventoryState(saved, catalog, state, error))
        return false;
    Buffer next;
    for (size_t slot = 0; slot < equipment.definitions.size(); ++slot)
        if (equipment.definitions[slot]) {
            if (!includeBagItems && state.locations[slot] != -1)
                continue;
            const uint32_t instance = static_cast<uint32_t>(player) * 128 + static_cast<uint32_t>(slot) + 1;
            std::array<uint16_t, 4> dyes{};
            const int group = equipment.types[slot] == 0 ? ColorIndex(slot) : -1;
            if (group >= 0)
                for (size_t channel = 0; channel < dyes.size(); ++channel)
                    dyes[channel] = colors[static_cast<size_t>(group) * 4 + channel];
            if (!ItemReport(instance, equipment.definitions[slot], equipment.types[slot], dyes, saved.guid,
                            state.bound[slot], player == 1, next) ||
                !next.U16(0x0112) || !next.VarU32(agent) || !next.VarU32(instance) ||
                !next.U8(state.locations[slot] == -1 ? uint8_t(2) : uint8_t(3)) ||
                !next.VarU32(state.locations[slot] == -1 ? static_cast<uint32_t>(slot)
                                                         : static_cast<uint32_t>(state.locations[slot])))
                return false;
        }
    out = std::move(next);
    return true;
}
bool EquipmentCatalog::InitializeInventory(Character& character, std::string& error) const {
    return china167409::InitializeInventory(*this, character, error);
}
bool EquipmentCatalog::Summary(const Character& saved, Buffer& out) const {
    InitialEquipment equipment;
    std::array<uint16_t, 24> colors{};
    Buffer base;
    if (!ResolveInitial(saved, equipment) || !Colors(saved, colors) || !Build204489Auth().Summary(saved, base) ||
        base.Size() != 159)
        return false;
    InventoryState state;
    std::string error;
    if (!LoadInventoryState(saved, *this, state, error))
        return false;
    std::vector<uint8_t> bytes(base.Data(), base.Data() + base.Size());
    // Native CompositeUtils resolves every armor SkinDef independently. Item
    // definition IDs and backpack/weapon IDs do not belong in these six records.
    for (size_t slot = 2; slot <= 7; ++slot) {
        if (state.locations[slot] != -1) {
            for (size_t byte = 0; byte < 12; ++byte)
                bytes[61 + (slot - 2) * 12 + byte] = 0;
            continue;
        }
        if (!equipment.definitions[slot])
            continue;
        const auto skin = appearances_.find(equipment.definitions[slot]);
        if (equipment.types[slot] != 0 || skin == appearances_.end() || skin->second.type != 0)
            return false;
        const size_t record = 61 + (slot - 2) * 12;
        for (unsigned byte = 0; byte < 4; ++byte)
            bytes[record + byte] = static_cast<uint8_t>(skin->second.skin >> (byte * 8));
        for (size_t channel = 0; channel < 4; ++channel) {
            const uint16_t color = colors[(slot - 2) * 4 + channel];
            bytes[record + 4 + channel * 2] = static_cast<uint8_t>(color);
            bytes[record + 5 + channel * 2] = static_cast<uint8_t>(color >> 8);
        }
    }
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < 155; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    crc = ~crc;
    for (unsigned byte = 0; byte < 4; ++byte)
        bytes[155 + byte] = static_cast<uint8_t>(crc >> (byte * 8));
    Buffer result;
    if (!result.Append(ByteView(bytes.data(), bytes.size())))
        return false;
    out = std::move(result);
    return true;
}
} // namespace china167409
} // namespace glide
