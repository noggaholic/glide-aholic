#include "inventory_state.h"
#include "starter_item_binding.h"
#include "inventory_wire.h"
#include "game.h"
#include "../../14505/server/world_actor.h"
#include "glide/files.h"
#include <algorithm>
#include <utility>

namespace glide {
namespace april14075 {
namespace {
const uint8_t armorSlots[6] = {2, 3, 5, 4, 6, 7};
uint32_t Definition(const june14505::RosterCharacter& original, size_t item) {
    return item < 6 ? original.armor[item] : item - 6 < original.weapons.size() ? original.weapons[item - 6].item : 0;
}
uint8_t EquipmentSlot(const june14505::RosterCharacter& original, size_t item) {
    return item < 6 ? armorSlots[item] : original.weapons[item - 6].slot;
}
bool Report(const june14505::RosterCharacter& original, size_t item, uint32_t instance, bool bound, Buffer& out) {
    const uint8_t flags = bound ? 0x20 : 0;
    return item < 6 ? InventoryArmorReport(instance, original.armor[item], original.dyes[item], out, flags)
                    : InventoryWeaponReport(instance, original.weapons[item - 6].item, out, flags);
}
class InventoryActions final : public WorldActions {
  public:
    InventoryActions(june14505::RosterCharacter original, InventoryState state, uint8_t player, uint16_t agent,
                     std::array<bool, 8> bindings, std::unique_ptr<june14505::WorldSkills> skills)
        : original_(std::move(original)), state_(state), bindings_(bindings), player_(player), agent_(agent),
          messages_(MakeGameMessages(agent)), skills_(std::move(skills)) {
        UpdateWeapons();
    }
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        Reader reader(input);
        uint16_t id = 0;
        if (!input.Valid() || !reader.U16(id))
            return WorldActionResult::Failed;
        if (id != 31 && id != 55 && id != 56)
            return skills_ ? skills_->Request(input, context, out) : WorldActionResult::Ignored;
        InventoryEquipRequest request;
        InventoryMoveRequest move;
        DecodeStatus decoded;
        if (id == 31) {
            decoded = DecodeInventoryMoveRequest(input, move);
            request.item = move.item;
            request.slot = move.slot;
            request.consumed = move.consumed;
        } else
            decoded =
                id == 55 ? DecodeInventoryEquipRequest(input, request) : DecodeInventoryUnequipRequest(input, request);
        if (decoded != DecodeStatus::Complete || request.consumed != input.size)
            return WorldActionResult::Failed;
        if (context.peerAgent != agent_)
            return WorldActionResult::Failed;
        const uint32_t first = static_cast<uint32_t>(player_) * 32 + 1;
        if (request.item < first || request.item >= first + 8)
            return Notice("That item does not belong to this character.", out);
        const size_t item = static_cast<size_t>(request.item - first);
        if (!Definition(original_, item))
            return Notice("That item does not belong to this character.", out);
        InventoryState next = state_;
        Buffer reply, peerReply;
        if (id == 31) {
            if (move.quantity != 1)
                return Notice("That item cannot be split.", out);
            if (move.location != 3 || request.slot >= 20)
                return Notice("That inventory slot is unavailable.", out);
            const auto change = state_.Move(item, request.slot, next);
            if (change == InventoryChangeResult::Unchanged)
                return Done(out);
            if (change != InventoryChangeResult::Changed)
                return Notice("That item is not in the bag.", out);
            // Native S99 swaps an occupied destination with the source cell.
            // Restrict both objects to this character's validated inventory.
            if (!InventoryItemMove(agent_, request.item, 3, static_cast<uint8_t>(request.slot), reply))
                return WorldActionResult::Failed;
        } else if (id == 55) {
            // The unchanged original recipe fixes race/profession/armor class;
            // each owned item can only return to its original compatible slot.
            if (request.slot != EquipmentSlot(original_, item))
                return Notice("That item cannot be equipped in this slot.", out);
            const auto change = state_.Equip(item, next, bindings_[item]);
            if (change == InventoryChangeResult::Unchanged)
                return Done(out);
            if (change != InventoryChangeResult::Changed)
                return Notice("That item is unavailable.", out);
            if (!InventoryItemRemove(agent_, request.item, reply) ||
                !Report(original_, item, request.item, next.bound[item], reply) ||
                !InventoryEquipmentAttach(agent_, request.item, EquipmentSlot(original_, item), reply) ||
                !Report(original_, item, request.item, next.bound[item], peerReply) ||
                !InventoryEquipmentAttach(agent_, request.item, EquipmentSlot(original_, item), peerReply))
                return WorldActionResult::Failed;
        } else {
            if (request.slot >= 20)
                return Notice("That inventory slot is unavailable.", out);
            const auto change = state_.Unequip(item, request.slot, next);
            if (change == InventoryChangeResult::Occupied)
                return Notice("That inventory slot is occupied.", out);
            if (change != InventoryChangeResult::Changed)
                return Notice("That item is not equipped.", out);
            // The dedicated equipment removal updates the paper doll, armor
            // attributes and observers. Both removal messages release the item;
            // restore its original definition/colors before the new placement.
            if (!InventoryEquipmentRemove(agent_, request.item, reply) ||
                !Report(original_, item, request.item, state_.bound[item], reply) ||
                !InventoryItemMove(agent_, request.item, 3, static_cast<uint8_t>(request.slot), reply) ||
                !InventoryEquipmentRemove(agent_, request.item, peerReply))
                return WorldActionResult::Failed;
        }
        WorldActionOutput output;
        output.sequences = 0;
        // Observers receive visible equipment only. A bag move is private;
        // unequipping removes the visible object without reporting its new cell.
        if (!output.owned.Append(reply.View()) || !output.peers.Append(peerReply.View()))
            return WorldActionResult::Failed;
        // Prepare both views before durable storage. Failed storage publishes
        // only an error and leaves locations unchanged, allowing a retry.
        if (!context.persistInventory || !context.persistInventory(next.Text(), next.BindingText()))
            return Notice("The inventory change could not be saved.", out);
        state_ = next;
        UpdateWeapons();
        out = std::move(output);
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
    WorldActionResult Done(WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    WorldActionResult Notice(const char* text, WorldActionOutput& out) {
        WorldActionOutput result;
        result.sequences = 0;
        Guid sender;
        if (!messages_ || !messages_->ServiceChat(text, sender, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    june14505::RosterCharacter original_;
    InventoryState state_;
    std::array<bool, 8> bindings_;
    uint8_t player_;
    uint16_t agent_;
    std::shared_ptr<const GameMessages> messages_;
    std::unique_ptr<june14505::WorldSkills> skills_;
};
} // namespace
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error) {
    Character fresh = character;
    fresh.profileFields.erase("inventory_locations");
    fresh.profileFields.erase("inventory_bound");
    june14505::RosterCharacter original;
    InventoryState state;
    if (!catalog.ResolveOriginal(fresh, original, error) || !InventoryState::Load(fresh, original, state, error))
        return false;
    std::array<bool, 8> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (Definition(original, i)) {
            const auto* rule = FindStarterItemBinding(Definition(original, i));
            if (!rule) {
                error = "Missing original April starting binding rule";
                return false;
            }
            // April's NoTrade/use rule applies to these initially equipped garments.
            bindings[i] = rule->onUse;
        }
    return state.StoreInitial(character, bindings, error);
}
bool InventoryState::Load(const Character& saved, const june14505::RosterCharacter& original, InventoryState& out,
                          std::string& error) {
    InventoryState next;
    std::array<uint32_t, 8> definitions{};
    if (original.weapons.size() > 2) {
        error = "Invalid April starting weapons";
        return false;
    }
    unsigned seen = 0;
    for (const auto& weapon : original.weapons) {
        const unsigned bit = weapon.slot == 24 ? 1u : weapon.slot == 29 ? 2u : 0u;
        if (!weapon.item || !bit || (seen & bit)) {
            error = "Invalid April starting weapons";
            return false;
        }
        seen |= bit;
    }
    for (size_t i = 0; i < definitions.size(); ++i)
        definitions[i] = Definition(original, i);
    // Previously saved April inventories contain six armor entries. Validate
    // that entire old state before adding the newly supported original weapons.
    // Retain every armor location/binding; mixed or malformed sizes fail closed.
    Character normalized = saved;
    const auto locations = saved.profileFields.find("inventory_locations");
    const auto bindings = saved.profileFields.find("inventory_bound");
    const bool oldLocations = locations != saved.profileFields.end() &&
                              std::count(locations->second.begin(), locations->second.end(), ',') == 5;
    const bool oldBindings =
        bindings != saved.profileFields.end() && std::count(bindings->second.begin(), bindings->second.end(), ',') == 5;
    if (oldLocations || oldBindings) {
        StarterInventoryState<6> legacy;
        if (!StarterInventoryState<6>::Load(saved, original.armor, 20, legacy, error))
            return false;
        if (locations != saved.profileFields.end()) {
            normalized.profileFields["inventory_locations"] =
                legacy.Text() + (definitions[6] ? ",e" : ",x") + (definitions[7] ? ",e" : ",x");
        }
        if (bindings != saved.profileFields.end())
            normalized.profileFields["inventory_bound"] = legacy.BindingText() + ",0,0";
    }
    if (!StarterInventoryState<8>::Load(normalized, definitions, 20, next, error))
        return false;
    out = next;
    error.clear();
    return true;
}
bool InitialInventoryEquipment(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                               Buffer& out, bool privateContents) {
    june14505::RosterCharacter original;
    InventoryState state;
    std::string error;
    Buffer result;
    if (!player || player == 255 || agent != 256u + player || !catalog.ResolveOriginal(saved, original, error) ||
        !InventoryState::Load(saved, original, state, error))
        return false;
    // Report all owned objects with their saved binding state. Put bag items
    // directly in the bag, without temporarily equipping them at entry.
    for (size_t i = 0; i < 8; ++i)
        if (Definition(original, i) && (privateContents || state.locations[i] == -1)) {
            const uint32_t instance = static_cast<uint32_t>(player) * 32 + static_cast<uint32_t>(i) + 1;
            if (!Report(original, i, instance, state.bound[i], result))
                return false;
            if (state.locations[i] == -1) {
                if (!InventoryEquipmentAttach(agent, instance, EquipmentSlot(original, i), result))
                    return false;
            } else if (!InventoryItemMove(agent, instance, 3, static_cast<uint8_t>(state.locations[i]), result))
                return false;
        }
    out = std::move(result);
    return true;
}
bool CreateInventoryActions(const CharacterCatalog& catalog, const Character& saved, uint8_t player, uint16_t agent,
                            std::unique_ptr<WorldActions>& out, std::string& error,
                            std::unique_ptr<june14505::WorldSkills> skills) {
    june14505::RosterCharacter original;
    InventoryState state;
    if (!player || player == 255 || agent != 256u + player || !catalog.ResolveOriginal(saved, original, error) ||
        !InventoryState::Load(saved, original, state, error)) {
        if (error.empty())
            error = "Invalid April inventory owner";
        return false;
    }
    std::array<bool, 8> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (Definition(original, i)) {
            const auto* rule = FindStarterItemBinding(Definition(original, i));
            if (!rule) {
                error = "Missing original April starter binding rule";
                return false;
            }
            bindings[i] = rule->onUse;
        }
    out.reset(new InventoryActions(std::move(original), state, player, agent, bindings, std::move(skills)));
    error.clear();
    return true;
}
} // namespace april14075
} // namespace glide
