#include "inventory_state.h"
#include "starter_item_binding.h"
#include "../../14075/server/inventory_wire.h"
#include "game.h"
#include <utility>

namespace glide {
namespace june14505 {
namespace {
bool JuneBuild(unsigned build) {
    return build == 14422 || build == 14505;
}
const uint8_t armorSlots[6] = {2, 3, 5, 4, 6, 7};
uint32_t Definition(const RosterCharacter& original, size_t item) {
    return item < 6 ? original.armor[item] : item - 6 < original.weapons.size() ? original.weapons[item - 6].item : 0;
}
uint8_t EquipmentSlot(const RosterCharacter& original, size_t item) {
    return item < 6 ? armorSlots[item] : original.weapons[item - 6].slot;
}
// April's validated serializers have the same field layouts. Translate only
// the explicitly verified semantic registration; never apply a numeric offset.
bool Translate(Buffer&& packet, uint16_t april, uint16_t june, Buffer& out) {
    Reader reader(packet.View());
    uint16_t id = 0;
    if (!reader.U16(id) || id != april)
        return false;
    packet.Data()[0] = static_cast<uint8_t>(june);
    packet.Data()[1] = static_cast<uint8_t>(june >> 8);
    return out.Append(packet.View());
}
bool Remove(uint16_t agent, uint32_t instance, bool equipped, Buffer& out) {
    Buffer packet;
    return (equipped ? april14075::InventoryEquipmentRemove(agent, instance, packet)
                     : april14075::InventoryItemRemove(agent, instance, packet)) &&
           Translate(std::move(packet), equipped ? 113 : 100, equipped ? 139 : 125, out);
}
bool Place(uint16_t agent, uint32_t instance, bool equipped, uint8_t slot, Buffer& out) {
    Buffer packet;
    return (equipped ? april14075::InventoryEquipmentAttach(agent, instance, slot, packet)
                     : april14075::InventoryItemMove(agent, instance, 3, slot, packet)) &&
           Translate(std::move(packet), equipped ? 98 : 99, equipped ? 123 : 124, out);
}
bool Report(const RosterCharacter& original, size_t item, uint32_t instance, bool bound, Buffer& out,
            bool owned = true) {
    // The original client checks bit 5 for soulbinding and bit 0 for binding
    // to its current character. A peer must not receive the owner's bit 0.
    const uint8_t flags = static_cast<uint8_t>(bound ? (owned ? 0x21 : 0x20) : 0);
    if (item < 6) {
        Buffer packet;
        return april14075::InventoryArmorReport(instance, original.armor[item], original.dyes[item], packet, flags) &&
               Translate(std::move(packet), 321, 408, out);
    }
    // Original June weapon report, already used by StartingEquipment.
    return out.U16(419) && out.VarU32(instance) && out.VarU32(Definition(original, item)) && out.U8(flags) &&
           out.VarU32(0) && out.VarU32(0) && out.U8(0) && out.VarU32(0);
}
class InventoryActions final : public WorldActions {
  public:
    InventoryActions(unsigned build, RosterCharacter original, InventoryState state, uint8_t player, uint16_t agent,
                     std::array<bool, 8> bindings, std::unique_ptr<WorldSkills> skills)
        : build_(build), original_(std::move(original)), state_(state), bindings_(bindings), player_(player),
          agent_(agent), skills_(std::move(skills)), messages_(MakeGameMessagesForBuild(build, agent)) {
        UpdateWeapons();
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        InventoryRequest request;
        const auto decoded = DecodeInventoryRequest(build_, input, request);
        if (decoded == DecodeStatus::Unknown)
            return skills_ ? skills_->Request(input, context, out) : WorldActionResult::Ignored;
        if (decoded != DecodeStatus::Complete || request.consumed != input.size || context.peerAgent != agent_)
            return WorldActionResult::Failed;
        const uint32_t first = static_cast<uint32_t>(player_) * 32 + 1;
        if (request.item < first || request.item >= first + 8 || !Definition(original_, request.item - first))
            return Notice("That item does not belong to this character.", out);
        const size_t item = static_cast<size_t>(request.item - first);
        InventoryState next = state_;
        WorldActionOutput result;
        result.sequences = 0;
        InventoryChangeResult change;
        if (request.kind == InventoryRequestKind::Move) {
            if (request.quantity != 1)
                return Notice("That item cannot be split.", out);
            if (request.location != 3 || request.slot >= 20)
                return Notice("That bag slot is unavailable.", out);
            change = state_.Move(item, request.slot, next);
        } else if (request.kind == InventoryRequestKind::Equip) {
            if (request.slot != EquipmentSlot(original_, item))
                return Notice("That item cannot be equipped in this slot.", out);
            change = state_.Equip(item, next, bindings_[item]);
        } else
            change = state_.Unequip(item, request.slot, next);
        if (change == InventoryChangeResult::Unchanged) {
            out = std::move(result);
            return WorldActionResult::Handled;
        }
        if (change != InventoryChangeResult::Changed)
            return Notice("That inventory change is unavailable.", out);
        if (request.kind == InventoryRequestKind::Move) {
            // Native placement swaps occupied bag cells without releasing items.
            if (!Place(agent_, request.item, false, static_cast<uint8_t>(request.slot), result.owned))
                return WorldActionResult::Failed;
        } else if (request.kind == InventoryRequestKind::Equip) {
            if (!Remove(agent_, request.item, false, result.owned) ||
                !Report(original_, item, request.item, next.bound[item], result.owned) ||
                !Place(agent_, request.item, true, EquipmentSlot(original_, item), result.owned) ||
                !Report(original_, item, request.item, next.bound[item], result.peers, false) ||
                !Place(agent_, request.item, true, EquipmentSlot(original_, item), result.peers))
                return WorldActionResult::Failed;
        } else {
            if (!Remove(agent_, request.item, true, result.owned) ||
                !Report(original_, item, request.item, next.bound[item], result.owned) ||
                !Place(agent_, request.item, false, static_cast<uint8_t>(request.slot), result.owned) ||
                !Remove(agent_, request.item, true, result.peers))
                return WorldActionResult::Failed;
        }
        // Preserve the current state and both client views if durable saving fails.
        if (!context.persistInventory || !context.persistInventory(next.Text(), next.BindingText()))
            return Notice("The inventory change could not be saved.", out);
        state_ = next;
        UpdateWeapons();
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        return skills_ ? skills_->Tick(context, out) : WorldActionResult::Ignored;
    }
    bool EvadesIncoming(uint64_t now) const override {
        return skills_ && skills_->EvadesIncoming(now);
    }
    bool BlocksIncoming(uint64_t now) const override {
        return skills_ && skills_->BlocksIncoming(now);
    }
    bool BlockIncoming(uint16_t source, const std::string& identity, uint64_t now) override {
        return skills_ && skills_->BlockIncoming(source, identity, now);
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        return skills_ && skills_->RequiresRequestTick(input, now);
    }
    uint64_t PeerEffectVersion() const override {
        return skills_ ? skills_->PeerEffectVersion() : 0;
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& c, const ReceivedWorldEffect& request,
                                            std::unique_ptr<PreparedWorldEffect>& out) override {
        return skills_ ? skills_->PrepareReceivedEffect(c, request, out) : WorldActionResult::Ignored;
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& c,
                                             const std::vector<ReceivedWorldEffect>& requests,
                                             std::unique_ptr<PreparedWorldEffect>& out) override {
        return skills_ ? skills_->PrepareReceivedEffects(c, requests, out) : WorldActionResult::Ignored;
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        return !skills_ || skills_->PeerCleanup(sequence, out);
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        return skills_ ? skills_->PeerEffect(context, seen, out) : WorldActionResult::Ignored;
    }

  private:
    void UpdateWeapons() {
        uint8_t mask = 0;
        for (size_t i = 0; i < original_.weapons.size(); ++i)
            if (state_.locations[6 + i] == -1)
                mask |= original_.weapons[i].slot == 24 ? 1 : 2;
        if (skills_)
            skills_->SetEquippedWeapons(mask);
    }
    WorldActionResult Notice(const char* text, WorldActionOutput& out) const {
        WorldActionOutput result;
        result.sequences = 0;
        Guid sender;
        if (!messages_ || !messages_->ServiceChat(text, sender, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    unsigned build_;
    RosterCharacter original_;
    InventoryState state_;
    std::array<bool, 8> bindings_;
    uint8_t player_;
    uint16_t agent_;
    std::unique_ptr<WorldSkills> skills_;
    std::shared_ptr<const GameMessages> messages_;
};
} // namespace
DecodeStatus DecodeInventoryRequest(unsigned build, ByteView input, InventoryRequest& out) {
    if (!JuneBuild(build))
        return DecodeStatus::Unknown;
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id = 0;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 33 && id != 56 && id != 57)
        return DecodeStatus::Unknown;
    InventoryRequest next;
    auto status = reader.VarU32(next.item);
    if (status != DecodeStatus::Complete)
        return status;
    if (id == 33) {
        uint8_t slot = 0;
        if (!reader.U8(next.quantity) || !reader.U8(next.location) || !reader.U8(slot))
            return DecodeStatus::Incomplete;
        next.slot = slot;
        if (!next.quantity || (next.location != 3 && next.location != 4 && next.location != 6 && next.location != 9))
            return DecodeStatus::Malformed;
    } else {
        next.kind = id == 56 ? InventoryRequestKind::Equip : InventoryRequestKind::Unequip;
        status = reader.VarU32(next.slot);
        if (status != DecodeStatus::Complete)
            return status;
        if (next.slot >= (id == 56 ? 59u : 160u))
            return DecodeStatus::Malformed;
    }
    if (!next.item)
        return DecodeStatus::Malformed;
    next.consumed = reader.Position();
    out = next;
    return DecodeStatus::Complete;
}
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error) {
    Character fresh = character;
    fresh.profileFields.erase("inventory_locations");
    fresh.profileFields.erase("inventory_bound");
    RosterCharacter original;
    InventoryState state;
    if (!JuneBuild(catalog.Build()) || !catalog.ResolveOriginal(fresh, original, error) ||
        !LoadInventoryState(fresh, original, state, error)) {
        error = "Cannot resolve original June starting inventory";
        return false;
    }
    std::array<bool, 8> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (Definition(original, i)) {
            const auto* rule = FindStarterItemBinding(catalog.Build(), Definition(original, i));
            if (!rule) {
                error = "Missing original June starting binding rule";
                return false;
            }
            // Original starter objects are initially equipped; use their own use rule.
            bindings[i] = rule->onUse;
        }
    return state.StoreInitial(character, bindings, error);
}
bool LoadInventoryState(const Character& saved, const RosterCharacter& original, InventoryState& out,
                        std::string& error) {
    if (original.weapons.size() > 2) {
        error = "Invalid June starter weapons";
        return false;
    }
    std::array<uint32_t, 8> definitions{};
    uint8_t slots = 0;
    for (size_t i = 0; i < 8; ++i)
        definitions[i] = Definition(original, i);
    for (const auto& weapon : original.weapons) {
        const uint8_t bit = weapon.slot == 24 ? 1 : weapon.slot == 29 ? 2 : 0;
        if (!weapon.item || !bit || (slots & bit)) {
            error = "Invalid June weapon slot";
            return false;
        }
        slots |= bit;
    }
    return InventoryState::Load(saved, definitions, 20, out, error);
}
bool InitialInventoryEquipment(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                               Buffer& out, bool privateContents) {
    RosterCharacter original;
    InventoryState state;
    std::string error;
    Buffer result;
    if (!JuneBuild(catalog.Build()) || !player || player == 255 || agent != 256u + player ||
        !catalog.ResolveOriginal(saved, original, error) || !LoadInventoryState(saved, original, state, error))
        return false;
    for (size_t i = 0; i < 8; ++i)
        if (Definition(original, i) && (privateContents || state.locations[i] == -1)) {
            const uint32_t instance = static_cast<uint32_t>(player) * 32 + static_cast<uint32_t>(i) + 1;
            if (!Report(original, i, instance, state.bound[i], result, privateContents) ||
                !Place(agent, instance, state.locations[i] == -1,
                       state.locations[i] == -1 ? EquipmentSlot(original, i) : static_cast<uint8_t>(state.locations[i]),
                       result))
                return false;
        }
    out = std::move(result);
    return true;
}
bool CreateInventoryActions(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                            std::unique_ptr<WorldSkills> skills, std::unique_ptr<WorldActions>& out,
                            std::string& error) {
    RosterCharacter original;
    InventoryState state;
    if (!JuneBuild(catalog.Build()) || !player || player == 255 || agent != 256u + player ||
        !catalog.ResolveOriginal(saved, original, error) || !LoadInventoryState(saved, original, state, error)) {
        if (error.empty())
            error = "Invalid June inventory owner";
        return false;
    }
    std::array<bool, 8> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (Definition(original, i)) {
            const auto* rule = FindStarterItemBinding(catalog.Build(), Definition(original, i));
            if (!rule) {
                error = "Missing original June starter binding rule";
                return false;
            }
            bindings[i] = rule->onUse;
        }
    out.reset(
        new InventoryActions(catalog.Build(), std::move(original), state, player, agent, bindings, std::move(skills)));
    error.clear();
    return true;
}
bool RestoreEquippedInventoryWeapons(const CharacterCatalog& catalog, const Character& saved, uint8_t player,
                                     uint16_t agent, Buffer& out) {
    RosterCharacter original;
    InventoryState state;
    std::string error;
    Buffer result;
    if (!JuneBuild(catalog.Build()) || !player || player == 255 || agent != 256u + player ||
        !catalog.ResolveOriginal(saved, original, error) || !LoadInventoryState(saved, original, state, error))
        return false;
    // The initial skillbar misses equipment notifications sent before its
    // listeners exist. Reuse native removal/report/attach after learning skills.
    // This updates only the owner's client view; durable inventory is unchanged.
    for (size_t i = 6; i < 8; ++i)
        if (Definition(original, i) && state.locations[i] == -1) {
            const uint32_t instance = static_cast<uint32_t>(player) * 32 + static_cast<uint32_t>(i) + 1;
            if (!Remove(agent, instance, true, result) || !Report(original, i, instance, state.bound[i], result) ||
                !Place(agent, instance, true, EquipmentSlot(original, i), result))
                return false;
        }
    out = std::move(result);
    return true;
}
} // namespace june14505
} // namespace glide
