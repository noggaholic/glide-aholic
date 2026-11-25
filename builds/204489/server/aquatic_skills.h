#pragma once
#include "aquatic_equipment.h"
#include "skill_phases.h"
#include <set>
#include <cstring>

namespace glide {
namespace modern {
// The supplementary files come from each build's own registered definitions.
// They are optional only when the entire equipment extension is absent.
template <class Skill>
bool LoadAquaticSkills(const std::wstring& directory, const std::array<AquaticEquipment, 10>& equipment,
                       std::map<unsigned, std::vector<Skill>>& out, std::string& error) {
    std::map<unsigned, std::vector<Skill>> result;
    bool present = false;
    for (unsigned p = 1; p <= 9; ++p) {
        result[p];
        present = present || equipment[p].item != 0;
    }
    if (!present) {
        out = std::move(result);
        return true;
    }
    error = "Invalid original aquatic skill catalog";
    Buffer bytes;
    if (!ReadFileBytes(directory + L"/aquatic-weapon-actions.txt", 32768, bytes))
        return false;
    std::istringstream actions(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, Skill> definitions;
    std::string line;
    while (std::getline(actions, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        Skill skill;
        unsigned moving = 0, ground = 0;
        std::string extra;
        std::istringstream row(line);
        if (!(row >> skill.skill >> skill.activationMs >> skill.animationMs >> skill.recoveryMs >> moving >>
              skill.rechargeMs >> ground >> skill.minimumGroundRange >> skill.maximumGroundRange) ||
            row >> extra || !skill.skill || skill.skill >= 0x400000 || moving > 1 || ground > 1 ||
            skill.animationMs > 65535 || skill.activationMs > skill.animationMs ||
            skill.recoveryMs > skill.animationMs - skill.activationMs || skill.rechargeMs > 3600000 ||
            !std::isfinite(skill.minimumGroundRange) || !std::isfinite(skill.maximumGroundRange) ||
            skill.minimumGroundRange < 0 || skill.maximumGroundRange < skill.minimumGroundRange)
            return false;
        skill.variableStages = moving != 0;
        skill.groundTargeted = ground != 0;
        skill.underwater = true;
        skill.equipmentSlot = 24;
        if (!definitions.emplace(skill.skill, skill).second)
            return false;
    }
    std::map<uint32_t, SkillPhases> phases;
    if (definitions.empty() || !LoadSkillPhases(directory + L"/aquatic-weapon-phases.txt", phases))
        return false;
    size_t matched = 0;
    for (auto& entry : definitions) {
        auto& skill = entry.second;
        const auto found = phases.find(skill.skill);
        if (!skill.animationMs) {
            if (found != phases.end())
                return false;
            continue;
        }
        if (found == phases.end())
            return false;
        unsigned duration = 0;
        bool moving = false;
        for (const auto& stage : found->second) {
            duration += stage.durationMs;
            moving = moving || stage.type != 0;
        }
        if (duration != skill.animationMs || moving != skill.variableStages)
            return false;
        skill.phases = found->second;
        ++matched;
    }
    if (matched != phases.size() || !ReadFileBytes(directory + L"/aquatic-weapon-skills.txt", 16384, bytes))
        return false;
    std::istringstream palettes(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::set<std::pair<unsigned, uint32_t>> identities;
    std::set<uint32_t> used;
    while (std::getline(palettes, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        unsigned profession = 0, slot = 0, minimum = 0, state = 0, chain = 0;
        uint32_t item = 0, id = 0, prior = 0;
        std::string extra;
        std::istringstream row(line);
        if (!(row >> profession >> item >> slot >> id >> minimum >> state >> prior >> chain) || row >> extra ||
            !profession || profession > 9 || item != equipment[profession].item || slot < 3 || slot > 7 || !minimum ||
            minimum > 80 || !state || state > 23 || prior >= 0x400000 || chain > 32 ||
            !identities.emplace(profession, id).second)
            return false;
        const auto found = definitions.find(id);
        if (found == definitions.end())
            return false;
        auto skill = found->second;
        skill.weapon = item;
        skill.slotType = slot;
        skill.minimumLevel = minimum;
        result[profession].push_back(std::move(skill));
        used.insert(id);
    }
    if (used.size() != definitions.size())
        return false;
    for (unsigned p = 1; p <= 9; ++p)
        if (result[p].empty())
            return false;
    out = std::move(result);
    error.clear();
    return true;
}

// C18's flags and motion mode are separate fields. Only mode 3 is deep water;
// mode 4 is the surface. The remaining valid modes select land equipment.
inline bool ReadMotionMedium(Reader& reader, uint8_t& medium, bool& valid) {
    uint32_t first = 0, second = 0, frame = 0;
    uint8_t flags = 0, mode = 0, last = 0;
    ByteView position;
    if (reader.VarU32(first) != DecodeStatus::Complete || reader.VarU32(second) != DecodeStatus::Complete ||
        !reader.Bytes(12, position) || reader.VarU32(frame) != DecodeStatus::Complete || !reader.U8(flags) ||
        !reader.U8(mode) || !reader.U8(last) || reader.Remaining())
        return false;
    float coordinates[3];
    std::memcpy(coordinates, position.data, 12);
    for (float value : coordinates)
        if (!std::isfinite(value))
            return false;
    valid = mode <= 5;
    medium = mode == 3 ? 1 : mode == 4 ? 2 : 0;
    return true;
}
inline bool WeaponSelection(uint16_t message, uint16_t actor, uint8_t medium, Buffer& out) {
    return out.U16(message) && out.VarU32(actor) && out.U8(medium == 1 ? 0 : 4);
}
} // namespace modern
} // namespace glide
