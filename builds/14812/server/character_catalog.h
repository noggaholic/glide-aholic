#pragma once
#include "world_maps.h"
#include "auth_response.h"
#include "glide/profile/character_codec.h"
#include <memory>
#include "../../14505/server/skill_catalog.h"
namespace glide {
namespace july14812 {
// Largest own set:24 elemental actions,four selectors and conditional Hurl.
constexpr size_t MaximumStarterSkills = 29;
constexpr size_t MaximumWeaponRecharges = 24;
struct StartingWeapon {
    uint32_t item = 0;
    uint8_t slot = 0;
};
struct CharacterRecipe {
    uint32_t skillLoadout = 0;
    uint8_t race = 0, sex = 0, profession = 0, level = 0;
    uint16_t map = 0;
    std::array<ArmorSource, 6> armor{};
    std::vector<StartingWeapon> weapons;
    uint8_t weaponSet = 0, professionState = 0;
    std::vector<uint32_t> skills;
};
class CharacterCatalog : public CharacterCodec {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    bool Load(const std::wstring&, std::string& error);
    bool Parse(ByteView, std::string& error);
    bool LoadSkills(const std::wstring& directory, std::string& error);
    const WorldMapCatalog& Maps() const {
        return maps_;
    }
    const june14505::SkillCatalog& Skills() const {
        return skills_;
    }
    const CharacterRecipe* Find(unsigned race, unsigned sex, unsigned profession, unsigned level) const;
    const CharacterRecipe* FindForCharacter(const Character& saved) const;
    bool Resolve(const Character&, RosterEntry&, std::string& error) const;
    bool ResolveOriginal(const Character&, RosterEntry&, std::string& error) const;
    bool Summary(const Character&, Buffer&) const override;
    size_t RecipeCount() const {
        return recipes_.size();
    }
    const std::vector<uint32_t>& Palette() const {
        return palette_;
    }

  private:
    WorldMapCatalog maps_;
    june14505::SkillCatalog skills_{14812};
    std::vector<CharacterRecipe> recipes_;
    std::vector<uint32_t> palette_;
};
bool LoadCharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>&, std::string& error);
bool SavedProfessionState(const Character& saved, uint8_t profession, uint8_t level, uint8_t& state);
} // namespace july14812
} // namespace glide
