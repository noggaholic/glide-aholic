#pragma once
#include "auth_response.h"
#include "glide/profile/character_codec.h"
#include <set>
#include <memory>

namespace glide {
namespace hot49284 {
struct CharacterRecipe {
    uint8_t race = 0, sex = 0, profession = 0, level = 0;
    uint16_t map = 0;
    std::array<uint32_t, 6> armor{};
    std::array<std::array<uint16_t, 4>, 6> colors{};
};
class CharacterCatalog : public CharacterCodec {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    bool Load(const std::wstring& path, std::string& error);
    bool Parse(ByteView bytes, std::string& error);
    const CharacterRecipe* Find(unsigned race, unsigned sex, unsigned profession, unsigned level) const;
    const CharacterRecipe* FindForCharacter(const Character& saved) const;
    bool Resolve(const Character&, RosterEntry&, std::string& error) const;
    bool ResolveOriginal(const Character&, RosterEntry&, std::string& error) const;
    bool Summary(const Character&, Buffer&) const override;
    size_t RecipeCount() const {
        return recipes_.size();
    }
    size_t ColorCount() const {
        return colors_.size();
    }
    bool HasColor(unsigned id) const {
        return colors_.count(id) != 0;
    }

  private:
    std::vector<CharacterRecipe> recipes_;
    std::set<uint32_t> colors_;
};
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>& out, std::string& error);
} // namespace hot49284
} // namespace glide
