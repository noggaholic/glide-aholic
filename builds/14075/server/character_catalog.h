#pragma once
#include "../../14505/server/character_catalog.h"

namespace glide {
namespace april14075 {
// April shares the 153-byte summary layout, with its own recipes and dye palette.
class CharacterCatalog final : public june14505::CharacterCatalog {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    CharacterCatalog() : june14505::CharacterCatalog(14075) {}
    bool LoadData(const std::wstring& dataRoot, std::string& error);
    bool ResolveOriginal(const Character& saved, june14505::RosterCharacter& out, std::string& error) const;
    bool Resolve(const Character& saved, june14505::RosterCharacter& out, std::string& error) const override;
};
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<glide::CharacterCodec>& out, std::string& error);
} // namespace april14075
} // namespace glide
