#include "skill_catalog.h"
#include "glide/files.h"
#include "glide/kv.h"
#include <cmath>
#include <locale>
#include <sstream>

namespace glide {
namespace june14505 {
namespace {
std::string Trim(const std::string& text) {
    const auto begin = text.find_first_not_of(" \t\r");
    return begin == std::string::npos ? std::string() : text.substr(begin, text.find_last_not_of(" \t\r") - begin + 1);
}
std::vector<std::string> Fields(const std::string& text) {
    std::vector<std::string> fields;
    size_t begin = 0;
    for (;;) {
        const auto end = text.find('|', begin);
        fields.push_back(Trim(text.substr(begin, end == std::string::npos ? end : end - begin)));
        if (end == std::string::npos)
            return fields;
        begin = end + 1;
    }
}
bool Number(const std::string& text, float& value) {
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    float result;
    stream >> result;
    if (!stream || !stream.eof() || !std::isfinite(result))
        return false;
    value = result;
    return true;
}
bool SameGroup(const SkillCandidate& a, const SkillCandidate& b) {
    return a.loadout == b.loadout && a.weaponSlot == b.weaponSlot && a.slot == b.slot &&
           a.professionState == b.professionState;
}
} // namespace
bool SkillCatalog::Load(const std::wstring& path, std::string& error) {
    Buffer data;
    if (!ReadFileBytes(path, 1024 * 1024, data)) {
        error = "Cannot read June skill catalog";
        return false;
    }
    return Parse(std::string(data.Size() ? reinterpret_cast<const char*>(data.Data()) : "", data.Size()), error);
}
const SkillCandidate* SkillCatalog::Select(uint32_t loadout, uint8_t weaponSlot, uint8_t slot,
                                           const std::string& professionState, uint32_t previousSkill,
                                           uint16_t chainCount) const {
    if (!loadout || (weaponSlot != 24 && weaponSlot != 29) || slot < 5 || slot > 9 || professionState.empty() ||
        (!previousSkill && chainCount))
        return nullptr;
    for (const auto& candidate : candidates_) {
        if (candidate.loadout != loadout || candidate.weaponSlot != weaponSlot || candidate.slot != slot)
            continue;
        if (candidate.professionState != "None" && candidate.professionState != professionState)
            continue;
        if (candidate.predecessor && (candidate.predecessor != previousSkill || chainCount < candidate.chainCount))
            continue;
        return &candidate;
    }
    return nullptr;
}
bool SkillCatalog::Parse(const std::string& text, std::string& error) {
    error = "Invalid June skill catalog";
    if ((build_ != 14075 && build_ != 14422 && build_ != 14505 && build_ != 14812) || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    std::istringstream stream(text);
    std::string line;
    unsigned header = 0, count = 0;
    bool open = false;
    SkillCandidate current;
    std::vector<SkillCandidate> result;
    while (std::getline(stream, line)) {
        if (line.size() > 2048)
            return false;
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto fields = Fields(line);
        if (header < 2) {
            if (fields.size() != 2 || fields[0] != (header ? "format" : "build") ||
                fields[1] != (header ? "1" : std::to_string(build_)))
                return false;
            ++header;
            continue;
        }
        if (fields[0] == "skill") {
            if (open || fields.size() != 15 || result.size() >= 1024)
                return false;
            const size_t indices[] = {1, 2, 3, 4, 5, 6, 10, 11, 12, 13, 14};
            unsigned values[11];
            for (size_t i = 0; i < 11; ++i)
                if (!ParseUnsigned(fields[indices[i]], UINT32_MAX, values[i]))
                    return false;
            if (!values[0] || (values[1] != 24 && values[1] != 29) || values[2] < 5 || values[2] > 9 || !values[3] ||
                values[5] > UINT16_MAX || (build_ != 14075 && (values[4] == 0) != (values[5] == 0)) ||
                values[6] > values[7] || values[7] > UINT16_MAX || values[9] > 1 ||
                (values[1] == 24) != (values[9] == 1) || values[10] > 256 || fields[7].empty() ||
                (!values[10] &&
                 ((" " + fields[8] + " ").find(" Instant ") == std::string::npos || values[6] || values[7])))
                return false;
            current = SkillCandidate{};
            current.loadout = values[0];
            current.weaponSlot = static_cast<uint8_t>(values[1]);
            current.slot = static_cast<uint8_t>(values[2]);
            current.skill = values[3];
            current.predecessor = values[4];
            current.chainCount = static_cast<uint16_t>(values[5]);
            current.loopMin = static_cast<uint16_t>(values[6]);
            current.loopMax = static_cast<uint16_t>(values[7]);
            current.recharge = values[8];
            current.underwater = values[9] != 0;
            count = values[10];
            current.professionState = fields[7];
            current.skillFlags = fields[8];
            current.abilityFlags = fields[9];
            open = true;
        } else if (fields[0] == "range") {
            if (!open || fields.size() != 3 || current.hasRange || !current.phases.empty() ||
                !Number(fields[1], current.rangeMin) || !Number(fields[2], current.rangeMax) || current.rangeMin < 0 ||
                current.rangeMax < current.rangeMin)
                return false;
            current.hasRange = true;
        } else if (fields[0] == "phase") {
            if (!open || fields.size() != 16 || current.phases.size() >= count)
                return false;
            unsigned values[4];
            for (size_t i = 0; i < 4; ++i)
                if (!ParseUnsigned(fields[i + 1], UINT32_MAX, values[i]))
                    return false;
            if (values[0] != current.phases.size() || values[1] > 1)
                return false;
            SkillAnimationPhase phase;
            phase.index = static_cast<uint8_t>(values[0]);
            phase.type = static_cast<uint8_t>(values[1]);
            phase.flags = values[2];
            phase.duration = values[3];
            if (!Number(fields[5], phase.speed) || phase.speed <= 0)
                return false;
            for (size_t i = 0; i < 10; ++i)
                if (!Number(fields[i + 6], phase.movement[i]))
                    return false;
            current.phases.push_back(phase);
        } else if (fields[0] == "end") {
            if (!open || fields.size() != 1 || current.phases.size() != count || (build_ == 14812 && !current.hasRange))
                return false;
            for (const auto& existing : result)
                if (SameGroup(existing, current) && existing.skill == current.skill)
                    return false;
            result.push_back(std::move(current));
            open = false;
        } else
            return false;
    }
    if (header != 2 || open || result.empty())
        return false;
    for (const auto& candidate : result)
        if (candidate.predecessor) {
            bool found = false;
            for (const auto& other : result)
                if (SameGroup(candidate, other) && other.skill == candidate.predecessor &&
                    other.skill != candidate.skill)
                    found = true;
            if (!found)
                return false;
        }
    candidates_ = std::move(result);
    error.clear();
    return true;
}
bool BuildSkillPlan(const SkillCandidate& skill, std::vector<SkillAnimationPhase>& out, std::string& error) {
    error = "Invalid June skill phase plan";
    const size_t count = skill.phases.size();
    if (!count && !skill.loopMin && !skill.loopMax &&
        (" " + skill.skillFlags + " ").find(" Instant ") != std::string::npos) {
        out.clear();
        error.clear();
        return true;
    }
    if (!count || count > 256 || skill.loopMin > skill.loopMax)
        return false;
    size_t begin = count, end = count;
    bool open = false, closed = false;
    for (size_t i = 0; i < count; ++i) {
        const auto& phase = skill.phases[i];
        if (phase.index != i || !phase.duration || phase.duration > UINT16_MAX || phase.type > 1 ||
            !std::isfinite(phase.speed) || phase.speed <= 0)
            return false;
        for (float value : phase.movement)
            if (!std::isfinite(value))
                return false;
        if (phase.flags & 16) {
            if (open || closed)
                return false;
            begin = i;
            open = true;
        }
        if (phase.flags & 32) {
            if (!open)
                return false;
            end = i + 1;
            open = false;
            closed = true;
        }
    }
    // LoopStepCount constrains a marked loop region; it does not create one.
    // Original earth skill 6224 has bounds 2,2 and one unmarked 2270ms phase.
    if (open)
        return false;
    std::vector<SkillAnimationPhase> result;
    uint32_t duration = 0;
    const auto append = [&](size_t index) {
        if (result.size() >= 256 || skill.phases[index].duration > 60000 - duration)
            return false;
        duration += skill.phases[index].duration;
        result.push_back(skill.phases[index]);
        return true;
    };
    if (!closed) {
        for (size_t i = 0; i < count; ++i)
            if (!append(i))
                return false;
    } else {
        for (size_t i = 0; i < begin; ++i)
            if (!append(i))
                return false;
        const size_t steps = skill.loopMin ? skill.loopMin : end - begin;
        for (size_t i = 0; i < steps; ++i)
            if (!append(begin + i % (end - begin)))
                return false;
        for (size_t i = end; i < count; ++i)
            if (!append(i))
                return false;
    }
    out = std::move(result);
    error.clear();
    return true;
}
} // namespace june14505
} // namespace glide
