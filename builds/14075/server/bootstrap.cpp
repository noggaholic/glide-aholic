#include "bootstrap.h"
#include "world_actor.h"

namespace glide {
namespace april14075 {
namespace {
bool ValidIdentity(const PeerIdentity& id) {
    return id.player && id.player < 255 && id.character == 256u + id.player && !id.firstBag && !id.itemBase;
}
class AprilBootstrap final : public GameBootstrap {
  public:
    AprilBootstrap(CharacterCatalog characters, MapGeometryCatalog maps)
        : characters_(std::move(characters)), maps_(std::move(maps)) {}
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        return WorldConstructionSteps(context, characters_, maps_, out);
    }
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        if (slot >= 254)
            return false;
        out = {static_cast<uint8_t>(slot + 1), static_cast<uint16_t>(257 + slot), 0, 0};
        return true;
    }
    bool BindSession(uint64_t owner, PeerIdentity& id) const override {
        return owner && ValidIdentity(id);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& id,
                   std::vector<BootstrapStep>& out) const override {
        const auto& character = context.entry.character;
        const auto* map = maps_.Find(character.map);
        if (!ValidIdentity(id) || context.catalog.build != 14075 || !context.catalog.Available(character.map) || !map ||
            context.catalog.maps.at(character.map).type != map->type)
            return false;
        BootstrapStep step;
        step.name = "April remote actor";
        if (!EquippedPeerActor(characters_, character, id.player, id.character, context.sequence, step.payload))
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
};
} // namespace
bool CreateBootstrap(CharacterCatalog characters, MapGeometryCatalog maps, std::unique_ptr<GameBootstrap>& out,
                     std::string& error) {
    if (characters.Build() != 14075 || maps.Build() != 14075 || characters.RecipeCount() != 48 ||
        characters.ColorCount() != 9 || maps.Count() != 747) {
        error = "Missing or mismatched April world data";
        return false;
    }
    std::unique_ptr<GameBootstrap> result(new AprilBootstrap(std::move(characters), std::move(maps)));
    out = std::move(result);
    error.clear();
    return true;
}
bool LoadBootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    CharacterCatalog characters;
    MapGeometryCatalog maps;
    if (!characters.LoadData(dataRoot, error) ||
        !characters.LoadWeaponSkills(dataRoot + L"/14075/weapon-skills.txt", error) || !maps.LoadData(dataRoot, error))
        return false;
    return CreateBootstrap(std::move(characters), std::move(maps), out, error);
}
} // namespace april14075
} // namespace glide
