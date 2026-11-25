#include "character_catalog.h"
#include "inventory_state.h"
#include "elemental_unlock_rules.h"
#include "glide/files.h"
#include "glide/profile/character_starting_level.h"
#include <algorithm>
#include <sstream>

namespace glide {
namespace june14505 {
bool SavedElementalStateForBuild(unsigned build, const Character& saved, uint8_t profession, unsigned level,
                                 uint8_t& state) {
    if ((build != 14075 && build != 14422 && build != 14505) || profession < 1 || profession > 8 || !level ||
        level > 80)
        return false;
    state = profession == 6 ? 1 : 0;
    if (profession != 6)
        return true;
    const auto at = saved.profileFields.find("profession_state");
    if (at == saved.profileFields.end())
        return true;
    const auto& text = at->second;
    if (text.size() != 1 || text[0] < '1' || text[0] > '4')
        return false;
    const uint8_t selected = static_cast<uint8_t>(text[0] - '0');
    if (level < kOwnJuneElementalUnlocks[selected - 1].minimumLevel)
        return false;
    state = selected;
    return true;
}
namespace {
std::string Trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r");
    return first == std::string::npos ? std::string() : text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}
std::vector<std::string> Parts(const std::string& text, char separator) {
    std::vector<std::string> result;
    size_t at = 0;
    for (;;) {
        const auto end = text.find(separator, at);
        result.push_back(Trim(text.substr(at, end == std::string::npos ? end : end - at)));
        if (end == std::string::npos)
            return result;
        at = end + 1;
    }
}
bool Field(const Character& saved, const char* key, unsigned maximum, unsigned& value) {
    const auto at = saved.profileFields.find(key);
    return at != saved.profileFields.end() && ParseUnsigned(at->second, maximum, value);
}
bool Values(const Character& saved, const char* key, size_t count, std::vector<unsigned>& values) {
    const auto at = saved.profileFields.find(key);
    if (at == saved.profileFields.end())
        return false;
    const auto parts = Parts(at->second, ',');
    if (parts.size() != count)
        return false;
    for (const auto& part : parts) {
        unsigned value;
        if (!ParseUnsigned(part, UINT16_MAX, value))
            return false;
        values.push_back(value);
    }
    return true;
}
} // namespace
const CharacterRecipe* CharacterCatalog::Find(unsigned race, unsigned sex, unsigned profession, unsigned level) const {
    for (const auto& recipe : recipes_)
        if (recipe.race == race && recipe.sex == sex && recipe.profession == profession && recipe.level == level)
            return &recipe;
    return nullptr;
}
const CharacterRecipe* CharacterCatalog::FindForCharacter(const Character& saved) const {
    unsigned race = 0, sex = 0, profession = 0, recipeLevel = 0;
    if (!StartingLoadoutLevel(saved, recipeLevel) || !Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) ||
        !Field(saved, "profession", 8, profession))
        return nullptr;
    return Find(race, sex, profession, recipeLevel);
}
bool CharacterCatalog::Load(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read June character catalog";
        return false;
    }
    return Parse(std::string(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size()), error);
}
bool CharacterCatalog::Parse(const std::string& text, std::string& error) {
    error = "Invalid June character catalog";
    if ((build_ != 14075 && build_ != 14422 && build_ != 14505) || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    CharacterCatalog result(build_);
    std::istringstream input(text);
    std::string line;
    bool build = false;
    while (std::getline(input, line)) {
        if (line.size() > 1024)
            return false;
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto parts = Parts(line, '|');
        if (!build) {
            if (parts.size() != 2 || parts[0] != "build" || parts[1] != std::to_string(build_))
                return false;
            build = true;
            continue;
        }
        if (parts[0] == "color") {
            unsigned id, index;
            if (parts.size() != 3 || !ParseUnsigned(parts[1], UINT16_MAX, id) || !id ||
                !ParseUnsigned(parts[2], UINT16_MAX, index) ||
                !result.colors_.emplace(id, static_cast<uint16_t>(index)).second)
                return false;
        } else if (parts[0] == "loadout") {
            if (parts.size() != 36 || result.recipes_.size() >= 256)
                return false;
            unsigned values[35];
            for (size_t i = 0; i < 35; ++i)
                if (!ParseUnsigned(parts[i + 1], i == 4 ? UINT32_MAX : UINT16_MAX, values[i]))
                    return false;
            if (values[0] > 4 || values[1] > 1 || !values[2] || values[2] > 8 || !values[3] || values[3] > 80 ||
                !values[4] || result.Find(values[0], values[1], values[2], values[3]))
                return false;
            CharacterRecipe recipe;
            recipe.race = values[0];
            recipe.sex = values[1];
            recipe.profession = values[2];
            recipe.level = values[3];
            recipe.loadout = values[4];
            for (size_t slot = 0; slot < 6; ++slot) {
                recipe.armor[slot] = static_cast<uint16_t>(values[5 + slot]);
                for (size_t channel = 0; channel < 4; ++channel)
                    recipe.colors[slot][channel] = static_cast<uint16_t>(values[11 + slot * 4 + channel]);
            }
            result.recipes_.push_back(recipe);
        } else
            return false;
    }
    if (!build || result.recipes_.empty() || result.colors_.empty())
        return false;
    for (const auto& recipe : result.recipes_)
        for (size_t slot = 0; slot < 6; ++slot) {
            if (!recipe.armor[slot])
                continue;
            for (unsigned color : recipe.colors[slot]) {
                const auto at = result.colors_.find(color);
                if (at == result.colors_.end() || at->second == UINT16_MAX) {
                    error = "Default character color has no palette index";
                    return false;
                }
            }
        }
    *this = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::LoadWeapons(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read June weapon catalog";
        return false;
    }
    return ParseWeapons(std::string(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size()),
                        error);
}
bool CharacterCatalog::ParseWeapons(const std::string& text, std::string& error) {
    error = "Invalid or incomplete starting weapon catalog";
    // Each build supplies its own file. April's own enum confirms slots 24/29.
    if ((build_ != 14075 && build_ != 14422 && build_ != 14505) || recipes_.empty() || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    std::map<unsigned, std::vector<StartingWeapon>> result;
    std::istringstream input(text);
    std::string line;
    bool build = false;
    while (std::getline(input, line)) {
        if (line.size() > 1024)
            return false;
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto parts = Parts(line, '|');
        if (!build) {
            if (parts.size() != 2 || parts[0] != "build" || parts[1] != std::to_string(build_))
                return false;
            build = true;
            continue;
        }
        unsigned loadout, slot, item;
        if (parts.size() != 3 || !ParseUnsigned(parts[0], UINT32_MAX, loadout) || !ParseUnsigned(parts[1], 255, slot) ||
            (slot != 24 && slot != 29) || !ParseUnsigned(parts[2], UINT32_MAX, item) || !item)
            return false;
        if (std::none_of(recipes_.begin(), recipes_.end(),
                         [loadout](const CharacterRecipe& recipe) { return recipe.loadout == loadout; }))
            return false;
        auto& entries = result[loadout];
        for (const auto& entry : entries)
            if (entry.slot == slot)
                return false;
        entries.push_back({static_cast<uint8_t>(slot), item});
    }
    if (!build)
        return false;
    // Every published starting recipe supplies one terrestrial and one aquatic weapon.
    for (const auto& recipe : recipes_)
        if (result[recipe.loadout].size() != 2)
            return false;
    for (auto& loadout : result)
        std::sort(loadout.second.begin(), loadout.second.end(),
                  [](const StartingWeapon& a, const StartingWeapon& b) { return a.slot < b.slot; });
    weapons_ = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::LoadWeaponSkills(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read June weapon skills";
        return false;
    }
    return ParseWeaponSkills(std::string(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size()),
                             error);
}
bool CharacterCatalog::ParseWeaponSkills(const std::string& text, std::string& error) {
    error = "Invalid or incomplete June weapon skills";
    if ((build_ != 14075 && build_ != 14422 && build_ != 14505) || recipes_.empty() || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    std::map<unsigned, std::vector<uint32_t>> result;
    std::istringstream input(text);
    std::string line;
    bool build = false;
    while (std::getline(input, line)) {
        if (line.size() > 1024)
            return false;
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto parts = Parts(line, '|');
        if (!build) {
            if (parts.size() != 2 || parts[0] != "build" || parts[1] != std::to_string(build_))
                return false;
            build = true;
            continue;
        }
        unsigned loadout, skill;
        if (parts.size() != 2 || !ParseUnsigned(parts[0], UINT32_MAX, loadout) ||
            !ParseUnsigned(parts[1], UINT32_MAX, skill) || !skill ||
            std::none_of(recipes_.begin(), recipes_.end(),
                         [loadout](const CharacterRecipe& recipe) { return recipe.loadout == loadout; }))
            return false;
        auto& entries = result[loadout];
        if (entries.size() >= 64 || std::find(entries.begin(), entries.end(), skill) != entries.end())
            return false;
        entries.push_back(skill);
    }
    if (!build)
        return false;
    for (const auto& recipe : recipes_)
        if (result[recipe.loadout].empty())
            return false;
    skills_ = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::ResolveOriginal(const Character& saved, RosterCharacter& out, std::string& error) const {
    error = "Invalid saved June character appearance or equipment";
    unsigned race = 0, sex = 0, profession = 0, scale = 0;
    std::vector<unsigned> armor, colors;
    std::vector<uint8_t> appearance;
    const auto at = saved.profileFields.find("appearance");
    if (!Field(saved, "race", 4, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 8, profession) ||
        !Field(saved, "body_scale", 15, scale) || at == saved.profileFields.end() ||
        !DecodeHex(at->second, 45, appearance) || appearance.size() != 45 || appearance[27] != race ||
        appearance[28] != scale || appearance[29] != sex || !Values(saved, "armor_items", 6, armor) ||
        !Values(saved, "armor_colors", 24, colors))
        return false;
    const auto* recipe = FindForCharacter(saved);
    if (!recipe) {
        error = "No published loadout for this June character";
        return false;
    }
    RosterCharacter result;
    result.guid = saved.guid;
    result.name = saved.name;
    result.map = saved.map;
    const auto weapons = weapons_.find(recipe->loadout);
    if (weapons != weapons_.end())
        result.weapons = weapons->second;
    const auto skills = skills_.find(recipe->loadout);
    if (skills != skills_.end())
        result.learnedSkills = skills->second;
    result.level = static_cast<uint8_t>(saved.level);
    result.profession = static_cast<uint8_t>(profession);
    std::copy(appearance.begin(), appearance.end(), result.appearance.begin());
    for (size_t slot = 0; slot < 6; ++slot) {
        if (armor[slot] != recipe->armor[slot])
            return false;
        result.armor[slot] = recipe->armor[slot];
        if (!result.armor[slot])
            continue;
        for (size_t channel = 0; channel < 4; ++channel) {
            const unsigned savedColor = colors[slot * 4 + channel];
            const auto color = colors_.find(savedColor ? savedColor : recipe->colors[slot][channel]);
            if (color == colors_.end() || color->second == UINT16_MAX) {
                error = "Saved color has no June equipment palette index";
                return false;
            }
            result.dyes[slot][channel] = color->second;
        }
    }
    Buffer check;
    if (!CharacterEntry(result, 1, check))
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::Resolve(const Character& saved, RosterCharacter& out, std::string& error) const {
    RosterCharacter original;
    if (!ResolveOriginal(saved, original, error))
        return false;
    if (build_ == 14422 || build_ == 14505) {
        uint8_t selectedState = 0;
        if (!SavedElementalStateForBuild(build_, saved, original.profession, original.level, selectedState)) {
            error = "Invalid saved June elemental state";
            return false;
        }
        InventoryState state;
        if (!LoadInventoryState(saved, original, state, error))
            return false;
        for (size_t i = 0; i < 6; ++i)
            if (state.locations[i] >= 0)
                original.armor[i] = 0;
        for (size_t i = original.weapons.size(); i > 0; --i)
            if (state.locations[5 + i] >= 0)
                original.weapons.erase(original.weapons.begin() + static_cast<ptrdiff_t>(i - 1));
    }
    out = std::move(original);
    error.clear();
    return true;
}
bool CharacterCatalog::InitializeInventory(Character& character, std::string& error) const {
    return june14505::InitializeInventory(*this, character, error);
}
bool CharacterCatalog::Summary(const Character& character, Buffer& out) const {
    RosterCharacter roster;
    std::string error;
    return Resolve(character, roster, error) && CharacterSummary(roster, out);
}
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>& out, std::string& error) {
    std::unique_ptr<CharacterCatalog> catalog(new CharacterCatalog);
    if (!catalog->Load(dataRoot + L"/14505/character-recipes.txt", error) ||
        !catalog->LoadWeapons(dataRoot + L"/14505/weapon-loadouts.txt", error))
        return false;
    out = std::move(catalog);
    error.clear();
    return true;
}
} // namespace june14505
} // namespace glide
