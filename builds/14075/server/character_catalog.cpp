#include "character_catalog.h"
#include "inventory_state.h"

namespace glide {
namespace april14075 {
bool CharacterCatalog::ResolveOriginal(const Character& saved, june14505::RosterCharacter& out,
                                       std::string& error) const {
    return june14505::CharacterCatalog::ResolveOriginal(saved, out, error);
}
bool CharacterCatalog::Resolve(const Character& saved, june14505::RosterCharacter& out, std::string& error) const {
    june14505::RosterCharacter next;
    InventoryState inventory;
    if (!ResolveOriginal(saved, next, error) || !InventoryState::Load(saved, next, inventory, error))
        return false;
    uint8_t selectedState = 0;
    if (!june14505::SavedElementalStateForBuild(14075, saved, next.profession, next.level, selectedState)) {
        error = "Invalid saved April elemental state";
        return false;
    }
    for (size_t i = 0; i < 6; ++i)
        if (inventory.locations[i] >= 0)
            next.armor[i] = 0;
    for (size_t i = next.weapons.size(); i > 0; --i)
        if (inventory.locations[5 + i] >= 0)
            next.weapons.erase(next.weapons.begin() + static_cast<ptrdiff_t>(i - 1));
    out = std::move(next);
    error.clear();
    return true;
}
bool CharacterCatalog::InitializeInventory(Character& character, std::string& error) const {
    return april14075::InitializeInventory(*this, character, error);
}
bool CharacterCatalog::LoadData(const std::wstring& dataRoot, std::string& error) {
    return Load(dataRoot + L"/14075/character-recipes.txt", error) &&
           LoadWeapons(dataRoot + L"/14075/weapon-loadouts.txt", error);
}
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<glide::CharacterCodec>& out, std::string& error) {
    std::unique_ptr<CharacterCatalog> catalog(new CharacterCatalog);
    if (!catalog->LoadData(dataRoot, error))
        return false;
    out = std::move(catalog);
    error.clear();
    return true;
}
} // namespace april14075
} // namespace glide
