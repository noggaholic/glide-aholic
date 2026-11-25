#include "bootstrap.h"
#include "world_actor.h"
#include "inventory_state.h"
#include <utility>
namespace glide {
namespace hot49284 {
namespace {
bool Valid(const PeerIdentity& id) {
    return id.player && id.player < 255 && id.character == 256u + id.player && !id.firstBag && id.itemBase &&
           id.itemBase <= 0xffffffe0 && !(id.itemBase & 31);
}
class HotBootstrap final : public GameBootstrap {
  public:
    CharacterCatalog characters;
    MapCatalog maps;
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
        return context.identity && Valid(*context.identity) &&
               WorldConstructionSteps(context, characters, maps, context.identity->itemBase, out);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& id,
                   std::vector<BootstrapStep>& out) const override {
        const auto& saved = context.entry.character;
        const auto* map = maps.Find(saved.map);
        if (!Valid(id) || context.catalog.build != 49284 || !context.catalog.Available(saved.map) || !map)
            return false;
        RosterEntry character;
        std::string error;
        if (!characters.Resolve(saved, character, error))
            return false;
        MapEntry world;
        world.map = map->id;
        world.name = saved.name;
        world.player = id.player;
        world.homeWorld = context.entry.account.homeWorld;
        world.worldClass = context.entry.account.worldClass;
        world.bounds = map->bounds;
        if (!NativePosition(saved.x, saved.y, saved.z, world.position))
            return false;
        WorldActor actor;
        actor.agent = id.character;
        actor.itemBase = id.itemBase;
        actor.sequence = context.sequence;
        if (!PrepareInventoryActor(saved, characters, actor, error))
            return false;
        BootstrapStep step;
        step.name = "HoT remote character";
        if (!AnnouncePeer(world, character, actor, step.payload))
            return false;
        std::vector<BootstrapStep> result;
        result.push_back(std::move(step));
        out = std::move(result);
        return true;
    }
    bool PeerRemove(const PeerIdentity& id, uint32_t sequence, Buffer& out) const override {
        return Valid(id) && RemovePeer(id.player, id.character, sequence, out);
    }
};
} // namespace
bool CreateBootstrap(CharacterCatalog characters, MapCatalog maps, std::unique_ptr<GameBootstrap>& out,
                     std::string& error) {
    error = "Invalid HoT world resources";
    if (characters.RecipeCount() != 54 || !maps.Find(15))
        return false;
    std::unique_ptr<HotBootstrap> result(new HotBootstrap);
    result->characters = std::move(characters);
    result->maps = std::move(maps);
    out = std::move(result);
    error.clear();
    return true;
}
bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    CharacterCatalog characters;
    MapCatalog maps;
    if (!characters.Load(data + L"/49284/construction.bin", error) || !maps.Load(data + L"/49284/maps.bin", error))
        return false;
    return CreateBootstrap(std::move(characters), std::move(maps), out, error);
}
} // namespace hot49284
} // namespace glide
