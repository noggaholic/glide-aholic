#include "bootstrap.h"
#include "profession_state.h"
#include "protocol.h"
#include "equipment_catalog.h"
#include "glide/files.h"
#include "../../204489/server/bootstrap.h"
#include <cstring>
#include <utility>

namespace glide {
namespace {
class EquippedChinaBootstrap final : public GameBootstrap {
  public:
    explicit EquippedChinaBootstrap(china167409::EquipmentCatalog catalog) : catalog_(std::move(catalog)) {}
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        return Build(context, nullptr, out);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& peer,
                   std::vector<BootstrapStep>& out) const override {
        return Build(context, &peer, out);
    }
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        return China167409Bootstrap().PeerSlot(slot, out);
    }
    bool BindSession(uint64_t owner, PeerIdentity& out) const override {
        return China167409Bootstrap().BindSession(owner, out);
    }
    bool PeerRemove(const PeerIdentity& peer, uint32_t sequence, Buffer& out) const override {
        return China167409Bootstrap().PeerRemove(peer, sequence, out);
    }

  private:
    china167409::EquipmentCatalog catalog_;
    bool Build(const BootstrapContext& context, const PeerIdentity* peer, std::vector<BootstrapStep>& out) const {
        std::vector<BootstrapStep> recipe;
        if (!(peer ? China167409Bootstrap().PeerSteps(context, *peer, recipe)
                   : China167409Bootstrap().Steps(context, recipe)))
            return false;
        bool elementalist = false;
        for (const auto& step : recipe)
            if (!std::strcmp(step.name, "character profession")) {
                Reader reader(step.payload.View());
                uint16_t id = 0;
                if (!reader.U16(id))
                    return false;
                elementalist = id == 454;
            }
        BootstrapStep attunement;
        attunement.name = "China initial elemental attunement";
        uint8_t initialState = 0;
        if (!china167409::SavedAttunement(context.entry.character, initialState) || (elementalist && !initialState))
            return false;
        // S454 constructs the controller but leaves its profession state at zero.
        // Original dagger palettes require an attunement (1..4). S477 selects
        // the saved element (state 1 by default) and refreshes the owner's bar.
        if (elementalist &&
            (!attunement.payload.U16(477) || !attunement.payload.VarU32(peer ? peer->character : uint16_t(57344)) ||
             !attunement.payload.VarU32(initialState)))
            return false;
        if (elementalist) {
            bool inserted = false;
            const char* anchor = peer ? "character profession" : "skillbar ready";
            for (auto at = recipe.begin(); at != recipe.end(); ++at)
                if (!std::strcmp(at->name, anchor)) {
                    recipe.insert(++at, std::move(attunement));
                    inserted = true;
                    break;
                }
            if (!inserted)
                return false;
        }
        if (!peer) {
            BootstrapStep support;
            support.name = "China original support skill palettes";
            if (!catalog_.SupportSkillPackets(context.entry.character, support.payload))
                return false;
            bool inserted = false;
            for (auto at = recipe.begin(); at != recipe.end(); ++at)
                if (!std::strcmp(at->name, "skillbar ready")) {
                    recipe.insert(at + 1, std::move(support));
                    inserted = true;
                    break;
                }
            if (!inserted)
                return false;
        }
        BootstrapStep equipment;
        equipment.name = "China original initial equipment";
        if (!china167409::InitialEquipmentPackets(catalog_, context.entry.character, peer ? peer->player : uint8_t(1),
                                                  peer ? peer->character : uint16_t(57344), equipment.payload, !peer))
            return false;
        BootstrapStep bag;
        bag.name = "China original initial bag";
        if (!china167409::InitialBagPackets(catalog_, peer ? peer->player : uint8_t(1),
                                            peer ? peer->character : uint16_t(57344), bag.payload))
            return false;
        BootstrapStep proficiency;
        proficiency.name = "China innate weapon permissions";
        if (!peer &&
            !china167409::InitialWeaponProficiencyPacket(catalog_, context.entry.character, proficiency.payload))
            return false;
        BootstrapStep learnedSkills;
        learnedSkills.name = "China original learned weapon skills";
        if (!peer && !china167409::InitialWeaponSkillPackets(catalog_, context.entry.character, learnedSkills.payload))
            return false;
        BootstrapStep weaponSet;
        weaponSet.name = "China active starter weapon set";
        // The original starter weapon occupies slot 29, the main hand of set 4.
        // The native inventory starts with set 11 (unset). Weapon comparisons
        // otherwise pass its invalid slot 68 to the equipment validator.
        if (!weaponSet.payload.U16(288) || !weaponSet.payload.VarU32(peer ? peer->character : uint16_t(57344)) ||
            !weaponSet.payload.U8(0) || !weaponSet.payload.U8(4))
            return false;
        BootstrapStep currentWeaponSet;
        currentWeaponSet.name = "China current weapon set";
        // S288 remembers the terrestrial selection. S284 actually activates it
        // and notifies the native skillbar; its initial active set is 11 (unset).
        if (!currentWeaponSet.payload.U16(284) ||
            !currentWeaponSet.payload.VarU32(peer ? peer->character : uint16_t(57344)) ||
            !currentWeaponSet.payload.U8(4))
            return false;
        if (!peer)
            for (auto& step : recipe)
                if (!std::strcmp(step.name, "inventory capacity")) {
                    // The original UI can query cells beyond the visible bag. Preserve its 480-cell backing storage.
                    Buffer capacity;
                    if (!capacity.U16(290) || !capacity.VarU32(57344) || !capacity.VarU32(480))
                        return false;
                    step.payload = std::move(capacity);
                }
        for (auto at = recipe.begin(); at != recipe.end(); ++at)
            if (!std::strcmp(at->name, "inventory ready")) {
                at = recipe.insert(at, std::move(currentWeaponSet));
                at = recipe.insert(at, std::move(weaponSet));
                at = recipe.insert(at, std::move(equipment));
                at = recipe.insert(at, std::move(bag));
                if (!peer) {
                    at = recipe.insert(at, std::move(learnedSkills));
                    recipe.insert(at, std::move(proficiency));
                }
                if (!peer) {
                    // Original China S201 updates the local player's wallet.
                    // Currency definition 1 is coins; both fields are VarU32.
                    BootstrapStep money;
                    money.name = "China account coins";
                    if (!money.payload.U16(201) || !money.payload.VarU32(1) ||
                        !money.payload.VarU32(context.entry.account.coins))
                        return false;
                    recipe.push_back(std::move(money));
                    BootstrapStep karma;
                    karma.name = "China account karma";
                    if (!karma.payload.U16(201) || !karma.payload.VarU32(2) ||
                        !karma.payload.VarU32(context.entry.account.karma))
                        return false;
                    recipe.push_back(std::move(karma));
                }
                out = std::move(recipe);
                return true;
            }
        return false;
    }
};
} // namespace
const GameBootstrap& China167409Bootstrap() {
    // Match the retained China world-entry recipe. Original bags are added by its catalog wrapper.
    static const auto bootstrap = CreateModernWorldBootstrap(
        ModernWorldDialect{167409, china167409::ServerMessageId, 0x0096, true, false, false, 300000, 9});
    return *bootstrap;
}
bool LoadChina167409Bootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    china167409::EquipmentCatalog catalog;
    if (!catalog.Load(dataRoot, error))
        return false;
    std::unique_ptr<GameBootstrap> next(new EquippedChinaBootstrap(std::move(catalog)));
    out = std::move(next);
    error.clear();
    return true;
}
} // namespace glide
