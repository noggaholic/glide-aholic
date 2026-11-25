#include "bootstrap.h"
#include "inventory_state.h"
#include "world_maps.h"
#include <cmath>
#include <utility>
namespace glide {
namespace july14812 {
bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer& out) {
    if (ticks >= 7200000 || !std::isfinite(rate) || rate < 0)
        return false;
    Buffer result;
    if (!result.U16(589) || !result.VarU32(ticks) || !result.F32(paused ? 0 : rate))
        return false;
    out = std::move(result);
    return true;
}
namespace {
bool ValidIdentity(const PeerIdentity& id) {
    return id.player && id.player < 255 && id.character == 256u + id.player && !id.firstBag && id.itemBase &&
           id.itemBase <= 0xffffffe0 && !(id.itemBase & 31);
}
bool Prepare(const BootstrapContext& context, const CharacterCatalog& characters, const PeerIdentity& id,
             MapEntry& world, RosterEntry& character, WorldActor& actor) {
    const auto& saved = context.entry.character;
    std::string error;
    const auto* geometry = characters.Maps().Find(saved.map);
    const auto map = context.catalog.maps.find(saved.map);
    if (!ValidIdentity(id) || context.catalog.build != 14812 || !context.catalog.Available(saved.map) || !geometry ||
        map == context.catalog.maps.end() || map->second.file != geometry->file || map->second.mft != geometry->mft ||
        context.entry.previousMap > UINT16_MAX || !context.sequence || context.sequence == UINT32_MAX ||
        !characters.Resolve(saved, character, error) || !PrepareActor(saved, characters, context.sequence, actor) ||
        !BindActor(actor, id.player, id.itemBase / 32) || actor.agent != id.character)
        return false;
    world.map = static_cast<uint16_t>(saved.map);
    world.player = id.player;
    world.name = saved.name;
    world.previousMap = static_cast<uint16_t>(context.entry.previousMap);
    world.homeWorld = context.entry.account.homeWorld;
    world.worldClass = context.entry.account.worldClass;
    world.bounds = geometry->bounds;
    return NativePosition(saved.x, saved.y, saved.z, world.position) && ValidMapEntry(world);
}
class JulyBootstrap final : public GameBootstrap {
  public:
    CharacterCatalog characters;
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        if (slot >= 254)
            return false;
        out = {static_cast<uint8_t>(slot + 1), static_cast<uint16_t>(257 + slot), 0, 0};
        return true;
    }
    bool BindSession(uint64_t owner, PeerIdentity& id) const override {
        if (!owner || owner > 0x07ffffff || !id.player || id.player >= 255 || id.character != 256u + id.player ||
            id.firstBag || id.itemBase)
            return false;
        id.itemBase = static_cast<uint32_t>(owner) * 32;
        return true;
    }
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        if (!context.identity)
            return false;
        MapEntry world;
        RosterEntry character;
        WorldActor actor;
        if (!Prepare(context, characters, *context.identity, world, character, actor))
            return false;
        // July core stats carry the owner karma after experience (native maximum 1,000,000).
        actor.karma = context.entry.account.karma;
        Buffer identity, spawn, load, owned, activation, begin, time;
        if (!EncodeMapEntry(world, MapEntryStep::Identity, identity) ||
            !EncodeMapEntry(world, MapEntryStep::Spawn, spawn) || !EncodeMapEntry(world, MapEntryStep::Load, load) ||
            !EncodeWorldActor(world, character, actor, false, owned) ||
            !ActivateWorldActor(world, actor.agent, actor.sequence + 1, activation) ||
            !EncodeMapEntry(world, MapEntryStep::Begin, begin) || !activation.Append(begin.View()) ||
            !TimeOfDay(context.timeTicks, context.timeRate, context.timePaused, time))
            return false;
        Buffer inventory;
        if (!InitialInventoryContents(context.entry.character, characters, context.identity->itemBase, actor.agent,
                                      inventory) ||
            !owned.Append(inventory.View()))
            return false;
        // July S132 updates this character inventory: agent, stored coins.
        if (!owned.U16(132) || !owned.VarU32(actor.agent) || !owned.VarU32(context.entry.account.coins))
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
        // MsgConn handshake order: C155 -> C154 -> C143 -> C151 -> C146.
        wait("July identity received", GameConfirmation::Hello);
        send("July map identity", std::move(identity));
        wait("July map accepted", GameConfirmation::MapContextReady);
        send("July spawn", std::move(spawn));
        wait("July spawn accepted", GameConfirmation::MapLoadAccepted);
        send("July map load", std::move(load));
        wait("July map loaded", GameConfirmation::MapLoaderReady);
        send("July character", std::move(owned));
        wait("July character ready", GameConfirmation::WorldBuilt);
        send("July controls and view", std::move(activation));
        send("July time of day", std::move(time));
        out = std::move(steps);
        return true;
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& id,
                   std::vector<BootstrapStep>& out) const override {
        MapEntry world;
        RosterEntry character;
        WorldActor actor;
        if (!Prepare(context, characters, id, world, character, actor))
            return false;
        BootstrapStep step;
        step.name = "July remote character";
        if (!AnnouncePeer(world, character, actor, step.payload))
            return false;
        std::vector<BootstrapStep> result;
        result.push_back(std::move(step));
        out = std::move(result);
        return true;
    }
    bool PeerRemove(const PeerIdentity& id, uint32_t sequence, Buffer& out) const override {
        return ValidIdentity(id) && RemovePeer(id.player, id.character, sequence, out);
    }
};
} // namespace
bool CreateBootstrap(CharacterCatalog characters, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    error = "Invalid July world construction resources";
    if (characters.RecipeCount() != 48 || characters.Palette().size() != 391)
        return false;
    std::unique_ptr<JulyBootstrap> result(new JulyBootstrap);
    result->characters = std::move(characters);
    out = std::move(result);
    error.clear();
    return true;
}
bool LoadBootstrap(const std::wstring& root, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.Load(root + L"/14812/construction.bin", error) || !catalog.LoadSkills(root + L"/14812", error))
        return false;
    return CreateBootstrap(std::move(catalog), out, error);
}
} // namespace july14812
} // namespace glide
