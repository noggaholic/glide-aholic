#include "character_catalog.h"
#include "inventory_state.h"
#include "glide/profile/character_starting_level.h"
#include "glide/files.h"
#include <algorithm>
#include <cstring>

namespace glide {
namespace hot49284 {
namespace {
bool Field(const Character& saved, const char* key, unsigned maximum, unsigned& value) {
    const auto at = saved.profileFields.find(key);
    return at != saved.profileFields.end() && ParseUnsigned(at->second, maximum, value);
}
bool Values(const Character& saved, const char* key, size_t count, unsigned maximum, std::vector<unsigned>& out) {
    const auto at = saved.profileFields.find(key);
    if (at == saved.profileFields.end())
        return false;
    size_t start = 0;
    for (size_t i = 0; i < count; ++i) {
        const auto end = at->second.find(',', start);
        unsigned value;
        if ((end == std::string::npos) != (i + 1 == count) ||
            !ParseUnsigned(at->second.substr(start, end == std::string::npos ? end : end - start), maximum, value))
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
    if (!ReadFileBytes(path, 16 + 2048 * 4 + 54 * 128, bytes)) {
        error = "Cannot read HoT character recipes";
        return false;
    }
    return Parse(bytes.View(), error);
}
bool CharacterCatalog::Parse(ByteView bytes, std::string& error) {
    error = "Invalid HoT character recipes";
    if (!bytes.Valid() || bytes.size < 16 || std::memcmp(bytes.data, "HOTCM01\0", 8))
        return false;
    Reader reader(bytes);
    ByteView magic;
    uint32_t build;
    uint16_t recipes, colors;
    if (!reader.Bytes(8, magic) || !reader.U32(build) || build != 49284 || !reader.U16(recipes) || recipes != 54 ||
        !reader.U16(colors) || !colors || colors > 2048 || bytes.size != 16u + colors * 4u + recipes * 128u)
        return false;
    CharacterCatalog parsed;
    uint32_t previous = 0;
    for (unsigned i = 0; i < colors; ++i) {
        uint32_t color;
        if (!reader.U32(color) || color <= previous || color > UINT16_MAX)
            return false;
        parsed.colors_.insert(color);
        previous = color;
    }
    for (unsigned i = 0; i < recipes; ++i) {
        CharacterRecipe recipe;
        uint16_t reserved;
        if (!reader.U8(recipe.race) || !reader.U8(recipe.sex) || !reader.U8(recipe.profession) ||
            !reader.U8(recipe.level) || !reader.U16(recipe.map) || !reader.U16(reserved) || reserved ||
            recipe.race < 1 || recipe.race > 3 || recipe.sex > 1 || recipe.profession < 1 || recipe.profession > 9 ||
            recipe.level != 1 || recipe.map != 15 ||
            parsed.Find(recipe.race, recipe.sex, recipe.profession, recipe.level))
            return false;
        for (size_t slot = 0; slot < 6; ++slot) {
            if (!reader.U32(recipe.armor[slot]))
                return false;
            for (auto& color : recipe.colors[slot]) {
                uint32_t id;
                if (!reader.U32(id) || (recipe.armor[slot] ? !parsed.HasColor(id) : id != 0))
                    return false;
                color = static_cast<uint16_t>(id);
            }
        }
        parsed.recipes_.push_back(recipe);
    }
    *this = std::move(parsed);
    error.clear();
    return true;
}
const CharacterRecipe* CharacterCatalog::FindForCharacter(const Character& saved) const {
    unsigned race = 0, sex = 0, profession = 0, level = 0;
    if (!StartingLoadoutLevel(saved, level) || !Field(saved, "race", 3, race) || !Field(saved, "sex", 1, sex) ||
        !Field(saved, "profession", 9, profession))
        return nullptr;
    return Find(race, sex, profession, level);
}
bool CharacterCatalog::ResolveOriginal(const Character& saved, RosterEntry& out, std::string& error) const {
    error = "Invalid saved HoT character appearance or equipment";
    unsigned race = 0, sex = 0, profession = 0, scale = 0;
    std::vector<unsigned> armor, colors;
    std::vector<uint8_t> appearance;
    const auto at = saved.profileFields.find("appearance");
    if (!Field(saved, "race", 3, race) || !Field(saved, "sex", 1, sex) || !Field(saved, "profession", 9, profession) ||
        !Field(saved, "body_scale", 15, scale) || at == saved.profileFields.end() ||
        !DecodeHex(at->second, 45, appearance) || appearance.size() != 45 || appearance[27] != race ||
        appearance[28] != scale || appearance[29] != sex || saved.map > UINT16_MAX ||
        !Values(saved, "armor_items", 6, UINT32_MAX, armor) || !Values(saved, "armor_colors", 24, UINT16_MAX, colors))
        return false;
    const auto* recipe = FindForCharacter(saved);
    if (!recipe) {
        error = "No published starting recipe for this HoT character";
        return false;
    }
    RosterEntry result;
    std::copy(saved.guid.bytes, saved.guid.bytes + 16, result.guid.begin());
    result.name = saved.name;
    auto& summary = result.summary;
    summary.map = static_cast<uint16_t>(saved.map);
    summary.level = static_cast<uint8_t>(saved.level);
    summary.profession = recipe->profession;
    std::copy(appearance.begin(), appearance.end(), summary.appearance.begin());
    for (size_t slot = 0; slot < 6; ++slot) {
        if (armor[slot] != recipe->armor[slot])
            return false;
        summary.armor[slot] = recipe->armor[slot];
        for (size_t channel = 0; channel < 4; ++channel) {
            const unsigned color = colors[slot * 4 + channel];
            if (!summary.armor[slot]) {
                if (color)
                    return false;
                continue;
            }
            const unsigned resolved = color ? color : recipe->colors[slot][channel];
            if (!HasColor(resolved))
                return false;
            summary.colors[slot][channel] = static_cast<uint16_t>(resolved);
        }
    }
    Buffer check;
    if (!CharacterEntry(result, 1, check))
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
bool CharacterCatalog::Resolve(const Character& saved, RosterEntry& out, std::string& error) const {
    RosterEntry original;
    InventoryState state;
    if (!ResolveOriginal(saved, original, error) || !LoadInventoryState(saved, *this, state, error))
        return false;
    for (size_t i = 0; i < 6; ++i)
        if (state.locations[i] != -1) {
            original.summary.armor[i] = 0;
            original.summary.colors[i].fill(0);
        }
    out = std::move(original);
    error.clear();
    return true;
}
bool CharacterCatalog::InitializeInventory(Character& character, std::string& error) const {
    return hot49284::InitializeInventory(*this, character, error);
}
bool CharacterCatalog::Summary(const Character& saved, Buffer& out) const {
    RosterEntry entry;
    std::string error;
    return Resolve(saved, entry, error) && EncodeSummary(entry.summary, out);
}
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>& out, std::string& error) {
    std::unique_ptr<CharacterCatalog> catalog(new CharacterCatalog);
    if (!catalog->Load(dataRoot + L"/49284/construction.bin", error))
        return false;
    out = std::move(catalog);
    error.clear();
    return true;
}
} // namespace hot49284
} // namespace glide
