#include "character_catalog.h"
#include "inventory_state.h"
#include "glide/profile/character_starting_level.h"
#include "glide/files.h"
#include <algorithm>
#include <cstring>
#include <sstream>
namespace glide {
namespace july14812 {
bool SavedProfessionState(const Character& saved, uint8_t profession, uint8_t level, uint8_t& state) {
    state = profession == 6 ? 1 : 0;
    const auto at = saved.profileFields.find("profession_state");
    if (at == saved.profileFields.end())
        return true;
    const auto& value = at->second;
    if (value.size() != 1 || value[0] < '0' || value[0] > '4')
        return false;
    const auto selected = static_cast<uint8_t>(value[0] - '0');
    if (profession != 6)
        return selected == 0;
    if (!selected || (selected >= 3 && level < 3) || (selected >= 4 && level < 6))
        return false;
    state = selected;
    return true;
}
namespace {
bool Field(const Character& saved, const char* key, unsigned max, unsigned& value) {
    const auto at = saved.profileFields.find(key);
    return at != saved.profileFields.end() && ParseUnsigned(at->second, max, value);
}
bool Values(const Character& saved, const char* key, size_t count, std::vector<unsigned>& out) {
    const auto at = saved.profileFields.find(key);
    if (at == saved.profileFields.end())
        return false;
    size_t start = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto end = at->second.find(',', start);
        unsigned value;
        if ((end == std::string::npos) != (i + 1 == count) ||
            !ParseUnsigned(at->second.substr(start, end == std::string::npos ? end : end - start), UINT32_MAX, value))
            return false;
        out.push_back(value);
        start = end == std::string::npos ? at->second.size() : end + 1;
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
bool CharacterCatalog::Load(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 12528, bytes)) {
        error = "Cannot read July character recipes";
        return false;
    }
    auto next = *this;
    if (!next.Parse(bytes.View(), error) ||
        !next.maps_.Load(path.substr(0, path.find_last_of(L"/\\")) + L"/server/map-geometry.bin", error))
        return false;
    *this = std::move(next);
    return true;
}
bool CharacterCatalog::LoadSkills(const std::wstring& directory, std::string& error) {
    auto next = *this;
    Buffer bytes;
    if (!next.skills_.Load(directory + L"/skill-catalog.txt", error))
        return false;
    error = "Invalid July skill loadout bindings";
    if (!ReadFileBytes(directory + L"/skill-loadouts.txt", 4096, bytes))
        return false;
    std::string text(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()), line;
    std::istringstream input(text);
    std::array<uint32_t, 9> bindings{};
    unsigned count = 0;
    bool header = false;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::replace(line.begin(), line.end(), '|', ' ');
        std::istringstream fields(line);
        std::string a, b, extra;
        if (!(fields >> a >> b) || fields >> extra)
            return false;
        if (!header) {
            if (a != "build" || b != "14812")
                return false;
            header = true;
            continue;
        }
        unsigned p = 0, id = 0;
        if (!ParseUnsigned(a, 8, p) || !p || !ParseUnsigned(b, UINT32_MAX, id) || !id || bindings[p])
            return false;
        for (auto existing : bindings)
            if (existing == id)
                return false;
        bindings[p] = id;
        ++count;
    }
    if (!header || count != 8 || next.recipes_.size() != 48)
        return false;
    for (auto& recipe : next.recipes_) {
        recipe.skillLoadout = bindings[recipe.profession];
        bool found = false;
        for (const auto& skill : next.skills_.Candidates())
            if (skill.loadout == recipe.skillLoadout) {
                found = true;
                // Both initial weapons, including the four original elemental palettes.
                if (skill.weaponSlot != 29 && skill.weaponSlot != 24)
                    continue;
                if (std::find(recipe.skills.begin(), recipe.skills.end(), skill.skill) == recipe.skills.end())
                    recipe.skills.push_back(skill.skill);
            }
        if (recipe.profession == 6)
            for (uint32_t skill : {518u, 520u, 521u, 522u})
                if (std::find(recipe.skills.begin(), recipe.skills.end(), skill) == recipe.skills.end())
                    recipe.skills.push_back(skill);
        if (!found || recipe.skills.size() > MaximumStarterSkills)
            return false;
    }
    *this = std::move(next);
    error.clear();
    return true;
}
bool CharacterCatalog::Parse(ByteView bytes, std::string& error) {
    error = "Invalid July character recipes";
    if (!bytes.Valid() || bytes.size != 12528 || std::memcmp(bytes.data, "JULYM01\0", 8))
        return false;
    Reader reader(bytes);
    ByteView magic;
    uint32_t build;
    uint16_t count, colors, width, reserved;
    if (!reader.Bytes(8, magic) || !reader.U32(build) || build != 14812 || !reader.U16(count) || count != 48 ||
        !reader.U16(colors) || colors != 391 || !reader.U16(width) || width != 228 || !reader.U16(reserved) || reserved)
        return false;
    CharacterCatalog parsed;
    uint32_t previous = 0;
    for (unsigned i = 0; i < colors; ++i) {
        uint32_t color;
        if (!reader.U32(color) || color <= previous)
            return false;
        parsed.palette_.push_back(color);
        previous = color;
    }
    for (unsigned i = 0; i < count; ++i) {
        CharacterRecipe recipe;
        if (!reader.U8(recipe.race) || !reader.U8(recipe.sex) || !reader.U8(recipe.profession) ||
            !reader.U8(recipe.level) || !reader.U16(recipe.map) || recipe.race < 1 || recipe.race > 3 ||
            recipe.sex > 1 || recipe.profession < 1 || recipe.profession > 8 || recipe.level != 1 || recipe.map != 15 ||
            parsed.Find(recipe.race, recipe.sex, recipe.profession, recipe.level))
            return false;
        for (auto& armor : recipe.armor) {
            if (!reader.U32(armor.item))
                return false;
            for (auto& id : armor.colorIds) {
                uint16_t index;
                if (!reader.U32(id) || (armor.item && !DyeIndex(parsed.palette_, id, index)))
                    return false;
            }
        }
        uint8_t weapons;
        if (!reader.U8(weapons) || !weapons || weapons > 6)
            return false;
        for (unsigned j = 0; j < 6; ++j) {
            StartingWeapon weapon;
            if (!reader.U32(weapon.item) || !reader.U8(weapon.slot))
                return false;
            if (j < weapons) {
                if (!weapon.item ||
                    !((weapon.slot >= 24 && weapon.slot <= 25) || (weapon.slot >= 29 && weapon.slot <= 32)))
                    return false;
                for (const auto& before : recipe.weapons)
                    if (before.slot == weapon.slot)
                        return false;
                recipe.weapons.push_back(weapon);
            } else if (weapon.item || weapon.slot)
                return false;
        }
        uint8_t skills;
        if (!reader.U8(recipe.weaponSet) || recipe.weaponSet != 4 || !reader.U8(recipe.professionState) ||
            recipe.professionState != (recipe.profession == 6 ? 1 : 0) || !reader.U8(skills) || skills > 17)
            return false;
        for (unsigned j = 0; j < 17; ++j) {
            uint32_t skill;
            if (!reader.U32(skill))
                return false;
            if (j < skills) {
                if (!skill || std::find(recipe.skills.begin(), recipe.skills.end(), skill) != recipe.skills.end())
                    return false;
                recipe.skills.push_back(skill);
            } else if (skill)
                return false;
        }
        parsed.recipes_.push_back(std::move(recipe));
    }
    if (reader.Remaining())
        return false;
    *this = std::move(parsed);
    error.clear();
    return true;
}
const CharacterRecipe* CharacterCatalog::FindForCharacter(const Character& saved) const {
    unsigned race = 0, sex = 0, profession = 0, level = 0;
    if (!StartingLoadoutLevel(saved, level) || !Field(saved, "race", 3, race) || !Field(saved, "sex", 1, sex) ||
        !Field(saved, "profession", 8, profession))
        return nullptr;
    return Find(race, sex, profession, level);
}
bool CharacterCatalog::ResolveOriginal(const Character& saved, RosterEntry& out, std::string& error) const {
    error = "Invalid saved July appearance, equipment or palette";
    unsigned race = 0, sex = 0, profession = 0, scale = 0;
    std::vector<unsigned> armor, colors;
    std::vector<uint8_t> appearance;
    const auto at = saved.profileFields.find("appearance");
    if (!Field(saved, "race", 3, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 8, profession) ||
        !Field(saved, "body_scale", 15, scale) || at == saved.profileFields.end() ||
        !DecodeHex(at->second, 45, appearance) || appearance.size() != 45 || appearance[27] != race ||
        appearance[28] != scale || appearance[29] != sex || !saved.map || saved.map > UINT16_MAX ||
        !Values(saved, "armor_items", 6, armor) || !Values(saved, "armor_colors", 24, colors))
        return false;
    const auto* recipe = FindForCharacter(saved);
    if (!recipe) {
        error = "No published recipe for this July character";
        return false;
    }
    RosterEntry result;
    if (!saved.native.empty() && !DecodeSummary(ByteView(saved.native.data(), saved.native.size()), result.summary))
        return false;
    std::copy(saved.guid.bytes, saved.guid.bytes + 16, result.guid.begin());
    result.name = saved.name;
    auto& summary = result.summary;
    summary.map = static_cast<uint16_t>(saved.map);
    summary.level = static_cast<uint8_t>(saved.level);
    summary.profession = recipe->profession;
    std::copy(appearance.begin(), appearance.end(), summary.appearance.begin());
    for (size_t slot = 0; slot < 6; ++slot) {
        if (armor[slot] != recipe->armor[slot].item)
            return false;
        summary.armor[slot] = armor[slot];
        for (size_t channel = 0; channel < 4; ++channel) {
            uint16_t index = 0;
            const uint32_t id =
                colors[slot * 4 + channel] ? colors[slot * 4 + channel] : recipe->armor[slot].colorIds[channel];
            if (armor[slot] && !DyeIndex(palette_, id, index))
                return false;
            summary.colors[slot][channel] = index;
        }
    }
    Buffer checked;
    if (result.name.empty() || !CharacterEntry(result, 1, checked))
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::Resolve(const Character& saved, RosterEntry& out, std::string& error) const {
    RosterEntry result;
    InventoryState state;
    if (!ResolveOriginal(saved, result, error) || !LoadInventoryState(saved, *this, state, error))
        return false;
    for (size_t i = 0; i < 6; ++i)
        if (state.locations[i] != -1) {
            result.summary.armor[i] = 0;
            result.summary.colors[i].fill(0);
        }
    Buffer checked;
    if (!CharacterEntry(result, 1, checked))
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::InitializeInventory(Character& character, std::string& error) const {
    return july14812::InitializeInventory(*this, character, error);
}
bool CharacterCatalog::Summary(const Character& saved, Buffer& out) const {
    RosterEntry entry;
    std::string error;
    return Resolve(saved, entry, error) && EncodeSummary(entry.summary, out);
}
bool LoadCharacterCodec(const std::wstring& root, std::unique_ptr<CharacterCodec>& out, std::string& error) {
    std::unique_ptr<CharacterCatalog> catalog(new CharacterCatalog);
    if (!catalog->Load(root + L"/14812/construction.bin", error))
        return false;
    out = std::move(catalog);
    error.clear();
    return true;
}
} // namespace july14812
} // namespace glide
