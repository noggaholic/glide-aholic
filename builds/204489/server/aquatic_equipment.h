#pragma once
#include "glide/files.h"
#include "glide/profile/starter_inventory_state.h"
#include <Windows.h>
#include <array>
#include <sstream>

namespace glide {
namespace modern {
struct AquaticEquipment {
    uint32_t item = 0, skin = 0;
    unsigned kind = 0;
};
// An optional extension keeps archived land-only catalog fixtures usable.
// If present, the file must describe every profession without duplicates.
inline bool LoadAquaticEquipment(const std::wstring& file, std::array<AquaticEquipment, 10>& out, std::string& error) {
    std::array<AquaticEquipment, 10> result{};
    if (GetFileAttributesW(file.c_str()) == INVALID_FILE_ATTRIBUTES) {
        const auto code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) {
            out = result;
            return true;
        }
        error = "Cannot inspect aquatic equipment catalog";
        return false;
    }
    Buffer bytes;
    if (!ReadFileBytes(file, 4096, bytes)) {
        error = "Cannot read aquatic equipment catalog";
        return false;
    }
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::string line;
    unsigned count = 0;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        unsigned profession = 0;
        AquaticEquipment item;
        std::string extra;
        if (!(row >> profession >> item.item >> item.skin >> item.kind) || row >> extra || profession < 1 ||
            profession > 9 || !item.item || item.item >= 0x400000 || !item.skin || item.skin >= 0x400000 ||
            item.kind < 19 || item.kind > 21 || result[profession].item) {
            error = "Invalid or duplicate aquatic equipment";
            return false;
        }
        result[profession] = item;
        ++count;
    }
    if (count != 9) {
        error = "Incomplete aquatic equipment catalog";
        return false;
    }
    out = result;
    return true;
}
inline bool LoadAquaticInventory(const Character& saved, const std::array<uint32_t, 68>& definitions, unsigned capacity,
                                 bool added, StarterInventoryState<68>& out, std::string& error) {
    if (StarterInventoryState<68>::Load(saved, definitions, capacity, out, error))
        return true;
    if (!added || !definitions[24] || saved.profileFields.find("inventory_locations") == saved.profileFields.end())
        return false;
    // Validate the whole old inventory before adding anything. Missing other
    // original items, duplicate cells and invalid binding remain errors.
    auto old = definitions;
    old[24] = 0;
    StarterInventoryState<68> migrated;
    if (!StarterInventoryState<68>::Load(saved, old, capacity, migrated, error))
        return false;
    if (migrated.locations[24] != -2 || migrated.bound[24])
        return false;
    migrated.locations[24] = -1;
    out = std::move(migrated);
    error.clear();
    return true;
}
} // namespace modern
} // namespace glide
