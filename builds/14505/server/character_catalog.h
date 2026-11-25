#pragma once
#include "auth_response.h"
#include "glide/profile/character_codec.h"
#include <map>
#include <memory>

namespace glide {
namespace june14505 {
bool SavedElementalStateForBuild(unsigned build, const Character& saved, uint8_t profession, unsigned level,
                                 uint8_t& state);
struct CharacterRecipe {
    unsigned race = 0, sex = 0, profession = 0, level = 0, loadout = 0;
    std::array<uint16_t, 6> armor{};
    std::array<std::array<uint16_t, 4>, 6> colors{};
};
class CharacterCatalog : public CharacterCodec {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    explicit CharacterCatalog(unsigned build = 14505) : build_(build) {}
    unsigned Build() const {
        return build_;
    }
    bool Load(const std::wstring& path, std::string& error);
    bool Parse(const std::string& text, std::string& error);
    bool LoadWeaponSkills(const std::wstring& path, std::string& error);
    bool ParseWeaponSkills(const std::string& text, std::string& error);
    bool LoadWeapons(const std::wstring& path, std::string& error);
    bool ParseWeapons(const std::string& text, std::string& error);
    const CharacterRecipe* Find(unsigned race, unsigned sex, unsigned profession, unsigned level) const;
    const CharacterRecipe* FindForCharacter(const Character& saved) const;
    virtual bool Resolve(const Character& saved, RosterCharacter& out, std::string& error) const;
    bool ResolveOriginal(const Character& saved, RosterCharacter& out, std::string& error) const;
    bool Summary(const Character& character, Buffer& out) const override;
    size_t RecipeCount() const {
        return recipes_.size();
    }
    size_t ColorCount() const {
        return colors_.size();
    }
    bool HasColor(unsigned id) const {
        return !id || colors_.count(id) != 0;
    }

  private:
    unsigned build_;
    std::map<unsigned, std::vector<StartingWeapon>> weapons_;
    std::map<unsigned, std::vector<uint32_t>> skills_;
    std::vector<CharacterRecipe> recipes_;
    std::map<unsigned, uint16_t> colors_;
};
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>& out, std::string& error);
} // namespace june14505
} // namespace glide
