#include "inventory_state.h"
#include "glide/profile/game_messages.h"
#include <utility>

namespace glide {
const GameMessages& Build204489Game();
namespace build204489 {
namespace {
bool Place(uint16_t agent, uint32_t item, unsigned slot, Buffer& out) {
    return out.U16(288) && out.VarU32(agent) && out.VarU32(item) && out.U8(3) && out.VarU32(slot);
}
bool Equip(uint16_t agent, uint32_t item, size_t slot, Buffer& out) {
    return out.U16(287) && out.VarU32(agent) && out.VarU32(item) && out.U8(static_cast<uint8_t>(slot));
}
bool Remove(uint16_t agent, uint32_t item, bool equipment, Buffer& out) {
    return out.U16(equipment ? uint16_t(309) : uint16_t(294)) && out.VarU32(agent) && out.VarU32(item);
}
class InventoryActions final : public WorldActions {
  public:
    InventoryActions(EquipmentCatalog catalog, Character saved, InventoryState state, InitialEquipment original,
                     PeerIdentity peer, std::unique_ptr<WeaponController> skills)
        : catalog_(std::move(catalog)), saved_(std::move(saved)), state_(state), original_(original), peer_(peer),
          skills_(std::move(skills)) {
        UpdateWeapons();
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        Reader reader(input);
        uint16_t id = 0;
        uint32_t instance = 0, slot = 0;
        uint8_t location = 3, quantity = 1;
        if (!input.Valid() || !reader.U16(id))
            return WorldActionResult::Failed;
        const bool equipment = id == 171 || id == 172;
        if (!equipment && id != 85 && id != 176)
            return skills_->Request(input, context, out);
        if (reader.VarU32(instance) != DecodeStatus::Complete || (id == 85 && !reader.U8(quantity)) ||
            (!equipment && !reader.U8(location)) || reader.VarU32(slot) != DecodeStatus::Complete ||
            reader.Remaining() || context.peerAgent != peer_.character)
            return WorldActionResult::Failed;
        const auto first = EquipmentCatalog::Instance(1, 0);
        if (instance < first || instance >= first + 68)
            return Notice("That item does not belong to this character.", out);
        const size_t item = static_cast<size_t>(instance - first);
        if (equipment && slot != item)
            return Notice("That item cannot be equipped in this slot.", out);
        if (!equipment && location != 3)
            return Notice("Choose a slot in this character's bag.", out);
        if (quantity != 1)
            return Notice("Choose one item to move.", out);
        InventoryState next;
        const auto change = equipment ? state_.Equip(item, next, catalog_.BindsOnEquip(original_.definitions[item]))
                                      : (id == 176 ? state_.Unequip(item, slot, next) : state_.Move(item, slot, next));
        if (change == InventoryChangeResult::Unchanged) {
            WorldActionOutput result;
            result.sequences = 0;
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (change != InventoryChangeResult::Changed)
            return Notice("That inventory change is unavailable.", out);
        WorldActionOutput result;
        result.sequences = 0;
        const auto remote = EquipmentCatalog::Instance(peer_.player, item);
        Buffer ownerReport, peerReport;
        if (equipment || id == 176) {
            if (!catalog_.ItemReport(saved_, item, instance, ownerReport, next.bound[item], true) ||
                (equipment && !catalog_.ItemReport(saved_, item, remote, peerReport, next.bound[item], false)))
                return WorldActionResult::Failed;
        }
        // Native removal releases the old object. Recreate the same logical item,
        // including its dyes and binding owner, before attaching it again.
        if (equipment) {
            if (!Remove(57344, instance, false, result.owned) || !result.owned.Append(ownerReport.View()) ||
                !Equip(57344, instance, item, result.owned) || !result.peers.Append(peerReport.View()) ||
                !Equip(peer_.character, remote, item, result.peers))
                return WorldActionResult::Failed;
        } else {
            if (id == 176 &&
                (!Remove(57344, instance, true, result.owned) || !result.owned.Append(ownerReport.View()) ||
                 !Remove(peer_.character, remote, true, result.peers)))
                return WorldActionResult::Failed;
            if (!Place(57344, instance, slot, result.owned))
                return WorldActionResult::Failed;
        }
        // Use June's durable commit before publishing either client view. A failed
        // save changes neither controller state nor appearance on other clients.
        if (!context.persistInventory || !context.persistInventory(next.Text(), next.BindingText()))
            return Notice("The inventory change could not be saved.", out);
        state_ = next;
        UpdateWeapons();
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        return skills_->RequiresRequestTick(input, now);
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        return skills_->Tick(context, out);
    }
    bool EvadesIncoming(uint64_t now) const override {
        return skills_->EvadesIncoming(now);
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& context, const ReceivedWorldEffect& effect,
                                            std::unique_ptr<PreparedWorldEffect>& prepared) override {
        return skills_->PrepareReceivedEffect(context, effect, prepared);
    }
    WorldActionResult PrepareStun(const WorldActionContext& c, std::unique_ptr<PreparedWorldControl>& out) override {
        return skills_->PrepareStun(c, out);
    }
    WorldActionResult PrepareDemonLeapImpact(const WorldActionContext& c, uint64_t utc,
                                             std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareDemonLeapImpact(c, utc, out);
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t utc,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareElementalBlast(c, utc, out);
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t utc,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareBurstStrength(c, utc, out);
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t utc,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareGazeDarkness(c, utc, out);
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t utc,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareEngagement(c, utc, out);
    }
    WorldActionResult BasicAttackRequest(const WorldActionContext& c, uint16_t target, uint32_t token,
                                         Buffer& out) const override {
        return skills_->BasicAttackRequest(c, target, token, out);
    }
    WorldActionResult PrepareStability(const WorldActionContext& c, uint64_t utc,
                                       std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareStability(c, utc, out);
    }
    WorldActionResult PrepareBanish(const WorldActionContext& c, uint64_t utc,
                                    std::unique_ptr<PreparedHostileEffects>& out) override {
        return skills_->PrepareBanish(c, utc, out);
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                             const std::vector<ReceivedWorldEffect>& effects,
                                             std::unique_ptr<PreparedWorldEffect>& prepared) override {
        return skills_->PrepareReceivedEffects(context, effects, prepared);
    }
    uint64_t PeerEffectVersion() const override {
        return skills_->PeerEffectVersion();
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        return skills_->PeerCleanup(sequence, out);
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        return skills_->PeerEffect(context, seen, out);
    }

  private:
    void UpdateWeapons() {
        auto equipped = original_.definitions;
        for (size_t slot = 0; slot < 68; ++slot)
            if (state_.locations[slot] != -1)
                equipped[slot] = 0;
        skills_->SetEquippedWeapons(equipped);
    }
    WorldActionResult Notice(const char* text, WorldActionOutput& out) const {
        WorldActionOutput result;
        result.sequences = 0;
        Guid context;
        if (!Build204489Game().ServiceChat(text, context, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    EquipmentCatalog catalog_;
    Character saved_;
    InventoryState state_;
    InitialEquipment original_;
    PeerIdentity peer_;
    std::unique_ptr<WeaponController> skills_;
};
} // namespace
bool CreateInventoryActions(const EquipmentCatalog& catalog, const Character& saved, const PeerIdentity& peer,
                            std::unique_ptr<WorldActions>& out, std::string& error) {
    InventoryState state;
    InitialEquipment original;
    std::unique_ptr<WeaponController> skills;
    if (!catalog.Inventory(saved, state, error) || !catalog.Resolve(saved, original) ||
        !CreateWeaponController(catalog, saved, peer, skills, error))
        return false;
    out.reset(new InventoryActions(catalog, saved, state, original, peer, std::move(skills)));
    error.clear();
    return true;
}
} // namespace build204489
} // namespace glide
