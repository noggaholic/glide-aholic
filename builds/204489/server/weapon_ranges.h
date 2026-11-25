#pragma once
#include "glide/files.h"
#include <cmath>
#include <map>
#include <set>
#include <sstream>

namespace glide {
namespace modern {
template <class Professions>
bool LoadWeaponTargetRanges(const std::wstring& file, Professions& professions, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(file, 8192, bytes)) {
        error = "Cannot read original weapon target ranges";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, std::pair<float, float>> ranges;
    std::string line;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t id = 0;
        float minimum = 0, maximum = 0;
        std::string extra;
        if (!(row >> id >> minimum >> maximum) || row >> extra || !id || id >= 0x400000 || !std::isfinite(minimum) ||
            !std::isfinite(maximum * maximum) || minimum < 0 || maximum < minimum ||
            !ranges.emplace(id, std::make_pair(minimum, maximum)).second) {
            error = "Invalid or duplicate weapon target range";
            return false;
        }
    }
    std::set<uint32_t> matched;
    for (auto& profession : professions)
        for (auto& skill : profession.second) {
            const auto found = ranges.find(skill.skill);
            if (found == ranges.end()) {
                error = "Missing original weapon target range";
                return false;
            }
            if (skill.groundTargeted &&
                (skill.minimumGroundRange != found->second.first || skill.maximumGroundRange != found->second.second)) {
                error = "Conflicting original ground and target ranges";
                return false;
            }
            skill.minimumGroundRange = found->second.first;
            skill.maximumGroundRange = found->second.second;
            matched.insert(skill.skill);
        }
    if (ranges.empty() || matched.size() != ranges.size()) {
        error = "Unmatched original weapon target range";
        return false;
    }
    return true;
}
} // namespace modern
} // namespace glide
