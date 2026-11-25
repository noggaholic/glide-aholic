#pragma once
#include "../../14505/server/character_catalog.h"

namespace glide {
namespace june14422 {
// The native layouts match June 11, but loadout IDs and colors come only from June 7.
class CharacterCatalog final : public june14505::CharacterCatalog {
  public:
    CharacterCatalog() : june14505::CharacterCatalog(14422) {}
    bool LoadData(const std::wstring& dataRoot, std::string& error);
};
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<glide::CharacterCodec>& out, std::string& error);
} // namespace june14422
} // namespace glide
