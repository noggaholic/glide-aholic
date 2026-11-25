#pragma once
#include "glide/profile/game_bootstrap.h"
#include "glide/profile/character_codec.h"
#include "../../204489/server/skill_phases.h"
#include "glide/profile/modern_support_loadout.h"
#include <array>
#include <map>

namespace glide {
namespace china167409 {
struct InitialEquipment {
    std::array<uint32_t, 68> definitions{};
    std::array<uint8_t, 68> types{};
};
struct InitialBagDefinition {
    uint32_t dataId = 0;
    unsigned capacity = 0;
};
struct WeaponProficiency {
    uint32_t traitId = 0;
    unsigned minimumLevel = 0;
    uint32_t mainHandMask = 0, offHandMask = 0, underwaterMask = 0;
};
struct InitialWeaponSkill {
    uint32_t weapon = 0, skill = 0;
    unsigned slotType = 0, minimumLevel = 0;
    unsigned activationMs = 0, animationMs = 0, recoveryMs = 0, rechargeMs = 0;
    bool underwater = false;
    bool variableStages = false;
    modern::SkillPhases phases;
    bool groundTargeted = false;
    float minimumGroundRange = 0, maximumGroundRange = 0;
    unsigned equipmentSlot = 29; // Main hand, or off hand for a one-handed weapon's final two skills.
    unsigned nativeSlot = 255;
    bool requiresWeapon = true;
};
class EquipmentCatalog final : public CharacterCodec {
  public:
    bool InitializeInventory(Character& character, std::string& error) const override;
    bool Load(const std::wstring& dataRoot, std::string& error);
    bool Resolve(const Character& character, const InitialEquipment*& out) const;
    bool ResolveInitial(const Character& character, InitialEquipment& out) const;
    bool HasAddedAquaticWeapon(const Character& character) const;
    bool ResolveWeaponProficiency(const Character& character, const WeaponProficiency*& out) const;
    bool ResolveInitialWeaponSkills(const Character& character, const std::vector<InitialWeaponSkill>*& out) const;
    bool ResolveAquaticWeaponSkills(const Character& character, const std::vector<InitialWeaponSkill>*& out) const;
    bool SupportSkillPackets(const Character& character, Buffer& out) const;
    bool Summary(const Character& character, Buffer& out) const override;
    size_t Count() const {
        return recipes_.size();
    }
    const InitialBagDefinition& InitialBag() const {
        return initialBag_;
    }

  private:
    struct BiographyChoice {
        unsigned response = 0, ordinal = 0, slot = 68;
        bool allowed = false;
        uint32_t definition = 0;
        uint8_t type = 0;
    };
    std::map<unsigned, InitialEquipment> recipes_;
    std::map<unsigned, bool> aquaticAdded_;
    std::map<unsigned, std::vector<BiographyChoice>> biography_;
    struct ItemAppearance {
        uint32_t skin = 0;
        uint8_t type = 0;
    };
    std::map<uint32_t, ItemAppearance> appearances_;
    InitialBagDefinition initialBag_;
    std::map<unsigned, WeaponProficiency> weaponProficiencies_;
    std::map<unsigned, std::vector<InitialWeaponSkill>> weaponSkills_, aquaticSkills_;
    modern::SupportPalettes supportPalettes_;
    modern::SupportUnlocks supportUnlocks_;
};
// Report each original object, then attach it to its original native slot.
bool InitialEquipmentPackets(const EquipmentCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                             Buffer& out, bool includeBagItems = true);
bool InitialBagPackets(const EquipmentCatalog& catalog, uint8_t player, uint16_t agent, Buffer& out);
// The original trait snapshot gives the owner its profession's innate weapon permissions.
bool InitialWeaponProficiencyPacket(const EquipmentCatalog& catalog, const Character& saved, Buffer& out);
// S585 is private learned-skill state for the owner Player, not an agent message.
bool InitialWeaponSkillPackets(const EquipmentCatalog& catalog, const Character& saved, Buffer& out);
// Restore the same original item after the native equipment removal releases it.
bool OriginalInventoryItemReport(const EquipmentCatalog& catalog, const Character& saved, unsigned slot,
                                 uint32_t instance, Buffer& out, bool bound = false, bool ownerView = true);
} // namespace china167409
} // namespace glide
