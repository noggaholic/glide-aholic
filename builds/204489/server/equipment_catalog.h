#pragma once
#include "glide/profile/character_codec.h"
#include "glide/profile/game_bootstrap.h"
#include "glide/profile/starter_inventory_state.h"
#include "skill_phases.h"
#include <array>
#include <map>
#include <memory>

namespace glide {
namespace build204489 {
using InventoryState = StarterInventoryState<68>;
struct InitialEquipment {
    std::array<uint32_t, 68> definitions{};
    std::array<uint8_t, 68> types{};
};
struct InitialWeaponSkill {
    uint32_t weapon = 0, skill = 0;
    unsigned slotType = 0, minimumLevel = 0, equipmentSlot = 29;
    unsigned activationMs = 0, animationMs = 0, recoveryMs = 0, rechargeMs = 0;
    bool underwater = false;
    bool variableStages = false, groundTargeted = false;
    modern::SkillPhases phases;
    float minimumGroundRange = 0, maximumGroundRange = 0;
    unsigned nativeSlot = 255, supportEnvironments = 0, knownSupportConditions = 0;
    bool requiresWeapon = true, timingKnown = true;
};
// Original default-variant dictionary. Context overrides and opaque value kinds
// remain evidence until their gameplay meaning is independently confirmed.
struct InitialSkillParameter {
    std::string key;
    uint32_t nameSpace = 0, type = 0, referenceType = 0, referenceId = 0;
    std::array<uint8_t, 40> raw{};
    bool Integer(int32_t& out) const;
    bool Float(float& out) const;
    bool ScaledInteger(int64_t& thousandths) const;
    bool Definition(uint32_t& definitionType, uint32_t& definitionId) const;
};
// Numeric amounts displayed by the original client. Applying them in combat
// additionally requires the skill's targets, conditions and effect schedule.
struct InitialSkillQuantity {
    uint32_t group = 0, index = 0, metadata = 0, mode = 0, modeFlags = 0, attribute = 0;
    bool requiresTrait = false, requiresPredicate = false;
    int32_t base = 0, count = 0;
    float attributeMultiplier = 0, levelExponent = 0, levelMultiplier = 0;
    bool Amount(unsigned level, int32_t attributeValue, int32_t& out) const;
};
class EquipmentCatalog final : public CharacterCodec {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    bool Load(const std::wstring& dataRoot, std::string& error);
    bool Resolve(const Character& saved, InitialEquipment& out) const;
    bool HasAddedAquaticWeapon(const Character& saved) const;
    bool Summary(const Character& saved, Buffer& out) const override;
    bool Inventory(const Character& saved, InventoryState& out, std::string& error) const;
    bool BindsOnEquip(uint32_t definition) const;
    bool ItemReport(const Character& saved, size_t slot, uint32_t instance, Buffer& out, bool bound = false,
                    bool ownerView = true) const;
    bool Packets(const Character& saved, uint8_t player, uint16_t agent, Buffer& out,
                 bool includeBagItems = true) const;
    bool WeaponPermissionPackets(const Character& saved, Buffer& out) const;
    bool SupportSkillPackets(const Character& saved, Buffer& out) const;
    bool ResolveInitialWeaponSkills(const Character& saved, const std::vector<InitialWeaponSkill>*& out) const;
    bool ResolveAquaticWeaponSkills(const Character& saved, const std::vector<InitialWeaponSkill>*& out) const;
    bool ResolveInitialSupportSkills(const Character& saved, const std::vector<InitialWeaponSkill>*& out) const;
    bool ResolveInitialSupportParameters(const Character& saved, uint32_t skill,
                                         const std::vector<InitialSkillParameter>*& out) const;
    const InitialSkillParameter* FindInitialSupportParameter(const Character& saved, uint32_t skill,
                                                             const std::string& key, uint32_t nameSpace) const;
    bool ResolveInitialSupportQuantities(const Character& saved, uint32_t skill,
                                         const std::vector<InitialSkillQuantity>*& out) const;
    bool ResolveRelatedSupportParameters(const Character& saved, uint32_t parentSkill, uint32_t relatedSkill,
                                         const std::vector<InitialSkillParameter>*& out) const;
    static uint32_t Instance(uint8_t player, size_t slot);

  private:
    std::map<unsigned, InitialEquipment> recipes_;
    std::map<unsigned, bool> aquaticAdded_;
    struct Appearance {
        uint32_t skin = 0;
        uint8_t type = 0;
        bool bindOnEquip = false;
    };
    std::map<uint32_t, Appearance> appearances_;
    struct WeaponProficiency {
        uint32_t trait = 0, underwaterMask = 0;
    };
    std::map<unsigned, WeaponProficiency> weaponProficiencies_;
    std::map<unsigned, std::vector<InitialWeaponSkill>> weaponSkills_, aquaticSkills_;
    std::map<unsigned, std::vector<InitialWeaponSkill>> supportSkills_;
    std::map<unsigned, std::array<std::array<uint32_t, 5>, 2>> supportPalettes_;
    std::map<unsigned, std::map<uint32_t, unsigned>> supportUnlocks_;
    std::map<uint32_t, std::vector<InitialSkillParameter>> supportParameters_;
    std::map<uint32_t, std::vector<InitialSkillParameter>> relatedSupportParameters_;
    std::map<uint32_t, std::vector<InitialSkillQuantity>> supportQuantities_;
};
} // namespace build204489
bool LoadBuild204489Bootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error);
bool LoadBuild204489CharacterCodec(const std::wstring& dataRoot, std::unique_ptr<CharacterCodec>& out,
                                   std::string& error);
} // namespace glide
