#pragma once
#include "glide/files.h"
#include <array>
#include <map>
#include <sstream>

namespace glide {
namespace modern {
using SupportPalettes = std::map<unsigned, std::array<std::array<uint32_t, 5>, 2>>;
using SupportUnlocks = std::map<unsigned, std::map<uint32_t, unsigned>>;
// Reuse the accepted Build 204489 loader and packet order with each build's own data.
inline bool LoadSupportLoadout(const std::wstring& root, unsigned build, SupportPalettes& palettes,
                               SupportUnlocks& unlocks, std::string& error) {
    if (build != 167409 && build != 204489)
        return false;
    const auto directory = root + L"/" + std::to_wstring(build) + L"/characters/";
    Buffer bytes;
    SupportPalettes nextPalettes;
    SupportUnlocks nextUnlocks;
    if (!ReadFileBytes(directory + L"initial-support-skill-palettes.txt", 16384, bytes)) {
        error = "Cannot read original support skill palettes";
        return false;
    }
    std::istringstream paletteRows(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::string line;
    unsigned paletteCount = 0;
    while (std::getline(paletteRows, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0, environment = 0, slot = 0;
        uint32_t palette = 0;
        std::string extra;
        if (!(row >> profession >> environment >> slot >> palette) || row >> extra || !profession || profession > 9 ||
            environment > 1 || slot > 4 || !palette || palette >= 0x400000 ||
            nextPalettes[profession][environment][slot]) {
            error = "Invalid original support skill palette";
            return false;
        }
        nextPalettes[profession][environment][slot] = palette;
        ++paletteCount;
    }
    if (paletteCount != 90 || nextPalettes.size() != 9) {
        error = "Incomplete original support skill palettes";
        return false;
    }
    if (!ReadFileBytes(directory + L"initial-support-skill-unlocks.txt", 32768, bytes)) {
        error = "Cannot read original support skill unlocks";
        return false;
    }
    std::istringstream unlockRows(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    while (std::getline(unlockRows, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0, minimum = 0;
        uint32_t skill = 0;
        std::string extra;
        if (!(row >> profession >> skill >> minimum) || row >> extra || !profession || profession > 9 || !skill ||
            skill >= 0x400000 || !minimum || minimum > 80 || !nextUnlocks[profession].emplace(skill, minimum).second) {
            error = "Invalid original support skill unlock";
            return false;
        }
    }
    if (nextUnlocks.size() != 9) {
        error = "Incomplete original support skill unlocks";
        return false;
    }
    palettes = std::move(nextPalettes);
    unlocks = std::move(nextUnlocks);
    return true;
}
inline bool SupportLoadoutPackets(unsigned build, const SupportPalettes& palettes, const SupportUnlocks& unlocks,
                                  unsigned profession, unsigned level, Buffer& out) {
    if ((build != 167409 && build != 204489) || !level || level > 80)
        return false;
    const auto p = palettes.find(profession);
    const auto u = unlocks.find(profession);
    if (p == palettes.end() || u == unlocks.end())
        return false;
    Buffer next;
    for (const auto& skill : u->second)
        if (level >= skill.second) {
            if (!next.U16(build == 167409 ? 585 : 605) || !next.VarU32(skill.first) || !next.VarU32(1))
                return false;
        }
    for (uint8_t environment = 0; environment < 2; ++environment)
        for (uint8_t slot = 0; slot < 5; ++slot) {
            if (!next.U16(build == 167409 ? 592 : 612) || !next.VarU32(p->second[environment][slot]) ||
                !next.U8(slot) || !next.U8(environment) || !next.VarU32(1))
                return false;
        }
    out = std::move(next);
    return true;
}
} // namespace modern
} // namespace glide
