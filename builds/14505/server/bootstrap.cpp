#include "bootstrap.h"
#include "entry_steps.h"
#include "world_actor.h"

namespace glide {
namespace june14505 {
namespace {
bool ValidIdentity(const PeerIdentity& id) {
    return id.player && id.player < 255 && id.character == 0x100u + id.player && !id.firstBag;
}
class JuneBootstrap final : public GameBootstrap {
  public:
    JuneBootstrap() = default;
    JuneBootstrap(CharacterCatalog characters, MapGeometryCatalog maps, WorldDiscovery discovery)
        : characters_(std::move(characters)), maps_(std::move(maps)), discovery_(std::move(discovery)) {}
    bool Load(const std::wstring& data, std::string& error) {
        return characters_.Load(data + L"/14505/character-recipes.txt", error) &&
               characters_.LoadWeapons(data + L"/14505/weapon-loadouts.txt", error) &&
               characters_.LoadWeaponSkills(data + L"/14505/weapon-skills.txt", error) &&
               maps_.Load(data + L"/14505/map-geometry.txt", error) &&
               LoadWorldDiscovery(data + L"/14505/discovery.txt", discovery_, error);
    }
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        if (slot >= 254)
            return false;
        out = {static_cast<uint8_t>(slot + 1), static_cast<uint16_t>(0x101 + slot), 0};
        return true;
    }
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        if (!context.identity || !ValidIdentity(*context.identity) || context.entry.previousMap > 65535)
            return false;
        // Retained exploration defaults from the previous June server.
        return WorldConstructionSteps(context, characters_, maps_, context.identity->player,
                                      context.identity->character, static_cast<uint16_t>(context.entry.previousMap),
                                      {{311, 179, 20}}, discovery_, out);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& id,
                   std::vector<BootstrapStep>& out) const override {
        if (!ValidIdentity(id) || context.catalog.build != characters_.Build() ||
            !context.catalog.Available(context.entry.character.map))
            return false;
        BootstrapStep step;
        step.name = "June remote actor";
        if (!PeerActor(characters_, context.entry.character, id.player, id.character, context.sequence, step.payload))
            return false;
        std::vector<BootstrapStep> result;
        result.push_back(std::move(step));
        out = std::move(result);
        return true;
    }
    bool PeerRemove(const PeerIdentity& id, uint32_t sequence, Buffer& out) const override {
        return ValidIdentity(id) && RemovePeer(id.player, id.character, sequence, out);
    }

  private:
    CharacterCatalog characters_;
    MapGeometryCatalog maps_;
    WorldDiscovery discovery_;
};
} // namespace
bool CreateBootstrapFromCatalog(CharacterCatalog characters, MapGeometryCatalog maps, WorldDiscovery discovery,
                                std::unique_ptr<GameBootstrap>& out, std::string& error) {
    if ((characters.Build() != 14422 && characters.Build() != 14505) || maps.Build() != characters.Build() ||
        discovery.build != characters.Build() || !characters.RecipeCount() || !characters.ColorCount() ||
        !maps.Count() || discovery.points.empty() || discovery.sectors.empty()) {
        error = "Missing or mismatched June world data";
        return false;
    }
    std::unique_ptr<GameBootstrap> result(
        new JuneBootstrap(std::move(characters), std::move(maps), std::move(discovery)));
    out = std::move(result);
    error.clear();
    return true;
}
bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    std::unique_ptr<JuneBootstrap> result(new JuneBootstrap);
    if (!result->Load(data, error))
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
} // namespace june14505
} // namespace glide
