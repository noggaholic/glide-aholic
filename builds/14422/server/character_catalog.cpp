#include "character_catalog.h"

namespace glide {
namespace june14422 {
bool CharacterCatalog::LoadData(const std::wstring& dataRoot, std::string& error) {
    return Load(dataRoot + L"/14422/character-recipes.txt", error);
}
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<glide::CharacterCodec>& out, std::string& error) {
    std::unique_ptr<CharacterCatalog> catalog(new CharacterCatalog);
    if (!catalog->LoadData(dataRoot, error) || !catalog->LoadWeapons(dataRoot + L"/14422/weapon-loadouts.txt", error))
        return false;
    out = std::move(catalog);
    error.clear();
    return true;
}
} // namespace june14422
} // namespace glide
