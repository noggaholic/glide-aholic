#include "entry_steps.h"
#include "world_actor.h"
#include "inventory_state.h"
#include "starter_skills.h"
#include <cmath>
#include <utility>
namespace glide {
namespace hot49284 {
bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out) {
    if (tick >= 7200000 || !std::isfinite(rate) || rate < 0)
        return false;
    Buffer result;
    if (!result.U16(895) || !result.VarU32(tick) || !result.F32(paused ? 0 : rate))
        return false;
    out = std::move(result);
    return true;
}
bool WorldConstructionSteps(const BootstrapContext& context, const CharacterCatalog& characters, const MapCatalog& maps,
                            uint32_t itemBase, std::vector<BootstrapStep>& out) {
    const auto& saved = context.entry.character;
    const auto* geometry = maps.Find(saved.map);
    if (context.catalog.build != 49284 || !context.catalog.Available(saved.map) || !geometry || !context.identity ||
        !context.identity->player || context.identity->character != 256u + context.identity->player ||
        !context.sequence || context.sequence == UINT32_MAX || context.entry.previousMap > 65535 || !itemBase ||
        itemBase > 0xffffffe0 || (itemBase & 31))
        return false;
    RosterEntry character;
    std::string error;
    if (!characters.Resolve(saved, character, error))
        return false;
    MapEntry world;
    world.player = context.identity->player;
    world.map = geometry->id;
    world.name = saved.name;
    world.homeWorld = context.entry.account.homeWorld;
    world.worldClass = context.entry.account.worldClass;
    world.previousMap = static_cast<uint16_t>(context.entry.previousMap);
    world.bounds = geometry->bounds;
    if (!NativePosition(saved.x, saved.y, saved.z, world.position))
        return false;
    WorldActor actor;
    actor.agent = context.identity->character;
    actor.sequence = context.sequence;
    actor.itemBase = itemBase;
    if (!PrepareInventoryActor(saved, characters, actor, error))
        return false;
    Buffer identity, spawn, load, owned, activation, time;
    if (!EncodeMapEntry(world, MapEntryStep::Identity, identity) ||
        !EncodeMapEntry(world, MapEntryStep::Spawn, spawn) || !EncodeMapEntry(world, MapEntryStep::Load, load) ||
        !EncodeWorldActor(world, character, actor, false, owned) ||
        !ActivateWorldActor(world, actor.agent, actor.sequence + 1, false, activation) ||
        !TimeOfDay(context.timeTicks, context.timeRate, context.timePaused, time))
        return false;
    Buffer inventory, learned, hammersPalette, dwarfHealingPalette, greatDwarfPalette, absorptionPalette,
        assassinPalette, thiefHealingPalette, engineerHealingPalette, elementalistHealingPalette;
    const auto* loadout = FindInventoryLoadout(saved, characters);
    if (!loadout || !StarterSkillGrants(*loadout, saved.level, world.player, learned))
        return false;
    if (loadout->profession == 9 && !StarterHammersPalette(world.player, hammersPalette))
        return false;
    if (loadout->profession == 9 && !StarterDwarfHealingPalette(world.player, dwarfHealingPalette))
        return false;
    if (loadout->profession == 9 && !StarterGreatDwarfPalette(world.player, greatDwarfPalette))
        return false;
    if (loadout->profession == 9 && !StarterPainAbsorptionPalette(world.player, absorptionPalette))
        return false;
    if (loadout->profession == 9 && !StarterAssassinTogglePalette(world.player, assassinPalette))
        return false;
    if (loadout->profession == 5 && !StarterThiefHealingPalette(world.player, thiefHealingPalette))
        return false;
    if (loadout->profession == 3 && !StarterEngineerHealingPalette(world.player, engineerHealingPalette))
        return false;
    if (loadout->profession == 6 && !StarterElementalistHealingPalette(world.player, elementalistHealingPalette))
        return false;
    if (!InitialInventoryContents(saved, characters, itemBase, actor.agent, inventory) ||
        !owned.Append(inventory.View()))
        return false;
    // S160 reports a currency definition id and its stored amount.
    // This owner-only message follows S299, which binds the local player.
    if (!owned.U16(160) || !owned.VarU32(1) || !owned.VarU32(context.entry.account.coins))
        return false;
    // Currency definition 2 is the account karma shown by the original wallet.
    if (!owned.U16(160) || !owned.VarU32(2) || !owned.VarU32(context.entry.account.karma))
        return false;
    std::vector<BootstrapStep> steps;
    const auto wait = [&](const char* name, GameConfirmation confirmation) {
        BootstrapStep step;
        step.kind = BootstrapStep::Kind::WaitFor;
        step.name = name;
        step.confirmation = confirmation;
        step.timeoutMs = 120000;
        steps.push_back(std::move(step));
    };
    const auto send = [&](const char* name, Buffer&& bytes) {
        BootstrapStep step;
        step.name = name;
        step.payload = std::move(bytes);
        steps.push_back(std::move(step));
    };
    // Retained world_session49284.inc gates C257,256,247,255,250 respectively.
    wait("HoT identity received", GameConfirmation::Hello);
    send("HoT map identity", std::move(identity));
    wait("HoT map accepted", GameConfirmation::MapContextReady);
    send("HoT spawn", std::move(spawn));
    wait("HoT spawn accepted", GameConfirmation::MapLoadAccepted);
    send("HoT map load", std::move(load));
    wait("HoT map loaded", GameConfirmation::MapLoaderReady);
    send("HoT character", std::move(owned));
    wait("HoT character ready", GameConfirmation::WorldBuilt);
    send("HoT learned starter skills", std::move(learned));
    // Owner-only: S299 and skillbar-ready precede these assignments, and the
    // learned bits are present before the palette refreshes the native bar.
    if (hammersPalette.Size())
        send("HoT Hammers support palette", std::move(hammersPalette));
    if (dwarfHealingPalette.Size())
        send("HoT dwarf healing support palette", std::move(dwarfHealingPalette));
    if (greatDwarfPalette.Size())
        send("HoT Rite support palette", std::move(greatDwarfPalette));
    if (absorptionPalette.Size())
        send("HoT pain absorption support palette", std::move(absorptionPalette));
    if (assassinPalette.Size())
        send("HoT assassin toggle support palette", std::move(assassinPalette));
    if (thiefHealingPalette.Size())
        send("HoT thief healing support palette", std::move(thiefHealingPalette));
    if (engineerHealingPalette.Size())
        send("HoT engineer aquatic healing support palette", std::move(engineerHealingPalette));
    if (elementalistHealingPalette.Size())
        send("HoT elementalist healing support palette", std::move(elementalistHealingPalette));
    send("HoT controls and view", std::move(activation));
    send("HoT time of day", std::move(time));
    out = std::move(steps);
    return true;
}
} // namespace hot49284
} // namespace glide
