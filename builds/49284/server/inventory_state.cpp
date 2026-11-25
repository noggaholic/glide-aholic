#include "inventory_state.h"
#include "profession_state.h"
#include "world_actions.h"
#include "starter_item_binding.h"
#include "glide/profile/build_profile.h"
#include "glide/profile/modern_health.h"
#include <utility>

namespace glide {
namespace hot49284 {
namespace {
uint32_t InventoryDefinition(const Hot49284Loadout& recipe, size_t item) {
    return item < 6 ? recipe.armor[item] : item - 6 < 6 ? recipe.weapons[item - 6].item : 0;
}
uint8_t InventoryEquipmentSlot(const Hot49284Loadout& recipe, size_t item) {
    return item < 6 ? static_cast<uint8_t>(item + 2) : recipe.weapons[item - 6].slot;
}
bool InventoryInstanceRange(uint32_t base, uint16_t agent) {
    return base && base <= 0xffffffe0u && !(base & 31) && agent >= 257 && agent <= 510;
}
bool InventoryRemove(uint16_t agent, uint32_t instance, bool equipped, Buffer& out) {
    return out.U16(equipped ? 228 : 214) && out.VarU32(agent) && out.VarU32(instance);
}
bool InventoryPlace(uint16_t agent, uint32_t instance, bool equipped, unsigned slot, Buffer& out) {
    if (equipped)
        return slot < 60 && out.U16(207) && out.VarU32(agent) && out.VarU32(instance) &&
               out.U8(static_cast<uint8_t>(slot));
    return slot < 160 && out.U16(208) && out.VarU32(agent) && out.VarU32(instance) && out.U8(3) && out.VarU32(slot);
}
bool InventoryReport(const Hot49284Loadout& recipe, const RosterEntry& original, size_t item, uint32_t instance,
                     bool bound, bool owned, Buffer& out) {
    const uint8_t flags = static_cast<uint8_t>(bound ? (owned ? 0x21 : 0x20) : 0);
    const uint32_t definition = InventoryDefinition(recipe, item);
    if (!definition)
        return false;
    if (item < 6) {
        if (!out.U16(655) || !out.VarU32(instance) || !out.VarU32(definition) || !out.U8(flags) ||
            !out.VarU32(original.summary.armor[item]))
            return false;
        for (uint16_t color : original.summary.colors[item])
            if (!out.U16(color))
                return false;
        return out.VarU32(0) && out.VarU32(0) && out.VarU32(0) && out.U8(0);
    }
    return out.U16(666) && out.VarU32(instance) && out.VarU32(definition) && out.U8(flags) && out.VarU32(0) &&
           out.VarU32(0) && out.VarU32(0) && out.VarU32(0) && out.U8(0) && out.VarU32(0) && out.U8(0);
}
class HotInventoryActions final : public WorldActions {
  public:
    HotInventoryActions(Hot49284Loadout recipe, RosterEntry original, InventoryState state,
                        std::array<bool, 12> bindings, uint32_t base, uint16_t agent,
                        std::unique_ptr<WeaponActions> actions)
        : recipe_(std::move(recipe)), original_(std::move(original)), state_(state), bindings_(bindings), base_(base),
          agent_(agent), actions_(std::move(actions)) {}
    WorldActionResult Request(ByteView input, const WorldActionContext& context, WorldActionOutput& out) override {
        InventoryRequest request;
        const auto decoded = DecodeInventoryRequest(input, request);
        if (decoded == DecodeStatus::Unknown)
            return actions_->Request(input, context, out);
        if (decoded != DecodeStatus::Complete || request.consumed != input.size || context.peerAgent != agent_)
            return WorldActionResult::Failed;
        if (request.item <= base_ || request.item > base_ + 12 ||
            !InventoryDefinition(recipe_, request.item - base_ - 1))
            return Notice("That item does not belong to this character.", out);
        const size_t item = request.item - base_ - 1;
        InventoryState next;
        InventoryChangeResult change;
        if (request.kind == june14505::InventoryRequestKind::Move) {
            if (request.quantity != 0 && request.quantity != 1)
                return Notice("That item cannot be split.", out);
            if (request.location != 3 || request.slot >= 160)
                return Notice("That bag slot is unavailable.", out);
            change = state_.Move(item, request.slot, next);
        } else if (request.kind == june14505::InventoryRequestKind::Equip) {
            if (request.slot != InventoryEquipmentSlot(recipe_, item))
                return Notice("That item cannot be equipped in this slot.", out);
            change = state_.Equip(item, next, bindings_[item]);
        } else {
            if (request.location != 3)
                return Notice("That bag location is unavailable.", out);
            change = state_.Unequip(item, request.slot, next);
        }
        if (change == InventoryChangeResult::Unchanged) {
            WorldActionOutput empty;
            empty.sequences = 0;
            out = std::move(empty);
            return WorldActionResult::Handled;
        }
        if (change != InventoryChangeResult::Changed)
            return Notice("That inventory change is unavailable.", out);
        WorldActionOutput result;
        result.sequences = 0;
        if (request.kind == june14505::InventoryRequestKind::Move) {
            for (size_t i = 0; i < 12; ++i)
                if (state_.locations[i] != next.locations[i] &&
                    !InventoryPlace(agent_, base_ + static_cast<uint32_t>(i) + 1, false,
                                    static_cast<unsigned>(next.locations[i]), result.owned))
                    return WorldActionResult::Failed;
        } else {
            for (size_t i = 0; i < 12; ++i)
                if (state_.locations[i] != next.locations[i] || state_.bound[i] != next.bound[i]) {
                    const uint32_t instance = base_ + static_cast<uint32_t>(i) + 1;
                    const bool before = state_.locations[i] == -1;
                    if (!InventoryRemove(agent_, instance, before, result.owned) ||
                        (before && !InventoryRemove(agent_, instance, true, result.peers)))
                        return WorldActionResult::Failed;
                }
            for (size_t i = 0; i < 12; ++i)
                if (state_.locations[i] != next.locations[i] || state_.bound[i] != next.bound[i]) {
                    const uint32_t instance = base_ + static_cast<uint32_t>(i) + 1;
                    const bool equipped = next.locations[i] == -1;
                    const unsigned slot =
                        equipped ? InventoryEquipmentSlot(recipe_, i) : static_cast<unsigned>(next.locations[i]);
                    if (!InventoryReport(recipe_, original_, i, instance, next.bound[i], true, result.owned) ||
                        !InventoryPlace(agent_, instance, equipped, slot, result.owned))
                        return WorldActionResult::Failed;
                    if (equipped &&
                        (!InventoryReport(recipe_, original_, i, instance, next.bound[i], false, result.peers) ||
                         !InventoryPlace(agent_, instance, true, slot, result.peers)))
                        return WorldActionResult::Failed;
                }
        }
        if (!context.persistInventory || !context.persistInventory(next.Text(), next.BindingText()))
            return Notice("The inventory change could not be saved. Your previous inventory is unchanged.", out);
        state_ = next;
        uint8_t equipped = 0;
        for (unsigned i = 0; i < 6; ++i)
            if (recipe_.weapons[i].item && state_.locations[i + 6] == -1)
                equipped |= static_cast<uint8_t>(1u << i);
        actions_->SetEquipment(equipped);
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    bool RequiresRequestTick(ByteView input, uint64_t now) const override {
        return actions_->RequiresRequestTick(input, now);
    }
    WorldActionResult Tick(const WorldActionContext& context, WorldActionOutput& out) override {
        return actions_->Tick(context, out);
    }
    bool EvadesIncoming(uint64_t now) const override {
        return actions_->EvadesIncoming(now);
    }
    WorldActionResult PrepareStun(const WorldActionContext& c, std::unique_ptr<PreparedWorldControl>& out) override {
        return actions_->PrepareStun(c, out);
    }
    WorldActionResult PrepareDemonLeapImpact(const WorldActionContext& c, uint64_t utc,
                                             std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareDemonLeapImpact(c, utc, out);
    }
    WorldActionResult PrepareElementalBlast(const WorldActionContext& c, uint64_t utc,
                                            std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareElementalBlast(c, utc, out);
    }
    WorldActionResult PrepareBurstStrength(const WorldActionContext& c, uint64_t utc,
                                           std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareBurstStrength(c, utc, out);
    }
    WorldActionResult PrepareGazeDarkness(const WorldActionContext& c, uint64_t utc,
                                          std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareGazeDarkness(c, utc, out);
    }
    WorldActionResult PrepareEngagement(const WorldActionContext& c, uint64_t utc,
                                        std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareEngagement(c, utc, out);
    }
    WorldActionResult BasicAttackRequest(const WorldActionContext& c, uint16_t target, uint32_t token,
                                         Buffer& out) const override {
        return actions_->BasicAttackRequest(c, target, token, out);
    }
    WorldActionResult PrepareStability(const WorldActionContext& c, uint64_t utc,
                                       std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareStability(c, utc, out);
    }
    WorldActionResult PrepareBanish(const WorldActionContext& c, uint64_t utc,
                                    std::unique_ptr<PreparedHostileEffects>& out) override {
        return actions_->PrepareBanish(c, utc, out);
    }
    WorldActionResult PrepareReceivedEffect(const WorldActionContext& context, const ReceivedWorldEffect& effect,
                                            std::unique_ptr<PreparedWorldEffect>& prepared) override {
        return actions_->PrepareReceivedEffect(context, effect, prepared);
    }
    WorldActionResult PrepareReceivedEffects(const WorldActionContext& context,
                                             const std::vector<ReceivedWorldEffect>& effects,
                                             std::unique_ptr<PreparedWorldEffect>& prepared) override {
        return actions_->PrepareReceivedEffects(context, effects, prepared);
    }

    bool PeerEffectsReady(uint64_t now) const override {
        return actions_->PeerEffectsReady(now);
    }
    uint64_t PeerEffectVersion() const override {
        return actions_->PeerEffectVersion();
    }
    bool PeerCleanup(uint32_t sequence, Buffer& out) const override {
        return actions_->PeerCleanup(sequence, out);
    }
    WorldActionResult PeerEffect(const WorldActionContext& context, bool seen, Buffer& out) const override {
        return actions_->PeerEffect(context, seen, out);
    }

  private:
    WorldActionResult Notice(const char* text, WorldActionOutput& out) const {
        WorldActionOutput result;
        result.sequences = 0;
        Guid zero;
        const auto* profile = BuildRegistry::Find(49284);
        if (!profile || !profile->Game().ServiceChat(text, zero, result.owned))
            return WorldActionResult::Failed;
        out = std::move(result);
        return WorldActionResult::Handled;
    }
    Hot49284Loadout recipe_;
    RosterEntry original_;
    InventoryState state_;
    std::array<bool, 12> bindings_;
    uint32_t base_;
    uint16_t agent_;
    std::unique_ptr<WeaponActions> actions_;
};
} // namespace
DecodeStatus DecodeInventoryRequest(ByteView input, InventoryRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 56 && id != 94 && id != 98)
        return DecodeStatus::Unknown;
    InventoryRequest request;
    request.kind = id == 56   ? june14505::InventoryRequestKind::Move
                   : id == 94 ? june14505::InventoryRequestKind::Equip
                              : june14505::InventoryRequestKind::Unequip;
    auto status = reader.VarU32(request.item);
    if (status != DecodeStatus::Complete)
        return status;
    if (id == 56 && !reader.U8(request.quantity))
        return DecodeStatus::Incomplete;
    if (id != 94 && !reader.U8(request.location))
        return DecodeStatus::Incomplete;
    status = reader.VarU32(request.slot);
    if (status != DecodeStatus::Complete)
        return status;
    request.consumed = reader.Position();
    out = request;
    return DecodeStatus::Complete;
}
const Hot49284Loadout* FindInventoryLoadout(const Character& saved, const CharacterCatalog& catalog) {
    const auto* recipe = catalog.FindForCharacter(saved);
    if (!recipe)
        return nullptr;
    for (const auto& candidate : PlayableLoadouts())
        if (candidate.race == recipe->race && candidate.sex == recipe->sex &&
            candidate.profession == recipe->profession)
            return &candidate;
    return nullptr;
}
bool InitializeInventory(const CharacterCatalog& catalog, Character& character, std::string& error) {
    Character fresh = character;
    fresh.profileFields.erase("inventory_locations");
    fresh.profileFields.erase("inventory_bound");
    const auto* recipe = FindInventoryLoadout(fresh, catalog);
    InventoryState state;
    if (!recipe || !LoadInventoryState(fresh, catalog, state, error)) {
        error = "Cannot resolve original HoT starting inventory";
        return false;
    }
    std::array<bool, 12> bindings{};
    for (size_t i = 0; i < bindings.size(); ++i)
        if (InventoryDefinition(*recipe, i)) {
            const auto* rule = FindStarterItemBinding(InventoryDefinition(*recipe, i));
            if (!rule || (rule->onAcquisition != 0 && rule->onAcquisition != 2) ||
                (rule->onUse != 0 && rule->onUse != 2)) {
                error = "Missing original HoT starting binding rule";
                return false;
            }
            bindings[i] = rule->onAcquisition == 2 || rule->onUse == 2;
        }
    return state.StoreInitial(character, bindings, error);
}
bool LoadInventoryState(const Character& saved, const CharacterCatalog& catalog, InventoryState& out,
                        std::string& error) {
    const auto* recipe = FindInventoryLoadout(saved, catalog);
    RosterEntry original;
    if (!recipe || !catalog.ResolveOriginal(saved, original, error))
        return false;
    std::array<uint32_t, 12> definitions{};
    for (size_t i = 0; i < 12; ++i)
        definitions[i] = InventoryDefinition(*recipe, i);
    if (InventoryState::Load(saved, definitions, 160, out, error))
        return true;
    // Older saved recipes have no aquatic item at index 7. Validate the whole
    // original inventory before adding that one item; retain every saved move.
    if (recipe->profession == 9 || recipe->weapons[1].slot != 24)
        return false;
    definitions[7] = 0;
    InventoryState legacy;
    if (!InventoryState::Load(saved, definitions, 160, legacy, error))
        return false;
    legacy.locations[7] = -1;
    legacy.bound[7] = false;
    out = legacy;
    error.clear();
    return true;
}
bool PrepareInventoryActor(const Character& saved, const CharacterCatalog& catalog, WorldActor& actor,
                           std::string& error) {
    InventoryState state;
    if (!LoadInventoryState(saved, catalog, state, error))
        return false;
    if (!modern::ReadHealthState(saved, actor.currentHealth, actor.health)) {
        error = "Invalid saved HoT health state";
        return false;
    }
    const auto conditionField = saved.profileFields.find("condition_effects");
    modern::ConditionState<49284> conditions;
    if (!conditions.Load(conditionField == saved.profileFields.end() ? std::string() : conditionField->second)) {
        error = "Invalid saved HoT conditions";
        return false;
    }
    const auto* recipe = FindInventoryLoadout(saved, catalog);
    if (!recipe || !SavedAttunement(saved, recipe->profession, actor.attunement)) {
        error = "Invalid saved HoT profession state";
        return false;
    }
    // The shared eight-point baseline belongs to initiative, not revenant
    // energy. Local cap50 is published as the native ceiling (a full ratio bar).
    LegendState legends;
    if (!legends.Load(saved, recipe->profession)) {
        error = "Invalid saved HoT legends";
        return false;
    }
    if (recipe->profession == 9) {
        actor.professionCurrent = 50;
        actor.professionMaximum = 50;
        actor.legends = {{legends.First(), legends.Second()}};
    }
    actor.inventoryLocations = state.locations;
    actor.inventoryBound = state.bound;
    return true;
}
bool InitialInventoryContents(const Character& saved, const CharacterCatalog& catalog, uint32_t base, uint16_t agent,
                              Buffer& out) {
    InventoryState state;
    RosterEntry original;
    std::string error;
    const auto* recipe = FindInventoryLoadout(saved, catalog);
    if (!recipe || !InventoryInstanceRange(base, agent) || !catalog.ResolveOriginal(saved, original, error) ||
        !LoadInventoryState(saved, catalog, state, error))
        return false;
    Buffer result;
    // Own bag: reuse the native 20-slot definition. Instance 31 is outside the
    // twelve starter items in this session's 32-instance allocation.
    // S220 opens one bag slot, S657 reports the bag, S219 installs it.
    if (!result.U16(220) || !result.VarU32(agent) || !result.U8(1) || !result.U16(657) || !result.VarU32(base + 31) ||
        !result.VarU32(22748) || !result.U8(0) || !result.U16(219) || !result.VarU32(agent) || !result.VarU32(0) ||
        !result.VarU32(base + 31))
        return false;
    for (size_t i = 0; i < 12; ++i)
        if (state.locations[i] >= 0) {
            const uint32_t instance = base + static_cast<uint32_t>(i) + 1;
            if (!InventoryReport(*recipe, original, i, instance, state.bound[i], true, result) ||
                !InventoryPlace(agent, instance, false, static_cast<unsigned>(state.locations[i]), result))
                return false;
        }
    // Keep the existing restored-equipment ordering; fresh loadouts retain their original bytes.
    const bool restored =
        saved.profileFields.count("inventory_locations") || saved.profileFields.count("inventory_bound");
    for (size_t i = 6; i < 12; ++i)
        if (restored && state.locations[i] == -1) {
            const uint32_t instance = base + static_cast<uint32_t>(i) + 1;
            if (!InventoryRemove(agent, instance, true, result) ||
                !InventoryReport(*recipe, original, i, instance, state.bound[i], true, result) ||
                !InventoryPlace(agent, instance, true, InventoryEquipmentSlot(*recipe, i), result))
                return false;
        }
    out = std::move(result);
    return true;
}
bool CreateInventoryActions(const Character& saved, const CharacterCatalog& catalog, uint32_t base, uint16_t agent,
                            std::unique_ptr<WorldActions>& out, std::string& error) {
    InventoryState state;
    RosterEntry original;
    const auto* recipe = FindInventoryLoadout(saved, catalog);
    if (!recipe || !InventoryInstanceRange(base, agent) || !catalog.ResolveOriginal(saved, original, error) ||
        !LoadInventoryState(saved, catalog, state, error))
        return false;
    uint8_t equipped = 0;
    for (unsigned i = 0; i < 6; ++i)
        if (recipe->weapons[i].item && state.locations[i + 6] == -1)
            equipped |= static_cast<uint8_t>(1u << i);
    auto actions = CreateWeaponActions(agent, saved, *recipe, equipped, error);
    if (!actions)
        return false;
    std::array<bool, 12> bindings{};
    for (size_t i = 0; i < 12; ++i) {
        const auto definition = InventoryDefinition(*recipe, i);
        if (!definition)
            continue;
        const auto* binding = FindStarterItemBinding(definition);
        if (!binding || (binding->onUse != 0 && binding->onUse != 2)) {
            error = "Missing original HoT starter-item binding rule";
            return false;
        }
        bindings[i] = binding->onUse == 2;
    }
    out.reset(new HotInventoryActions(*recipe, std::move(original), state, bindings, base, agent, std::move(actions)));
    error.clear();
    return true;
}
} // namespace hot49284
} // namespace glide
