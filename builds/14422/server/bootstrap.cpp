#include "bootstrap.h"
#include "character_catalog.h"
#include "map_geometry.h"
#include "../../14505/server/bootstrap.h"
#include "../../14505/server/world_environment.h"

namespace glide {
namespace june14422 {
bool LoadBootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error) {
    CharacterCatalog characters;
    MapGeometryCatalog maps;
    june14505::WorldDiscovery discovery;
    const auto directory = dataRoot + L"/14422/";
    if (!characters.LoadData(dataRoot, error) || !characters.LoadWeapons(directory + L"weapon-loadouts.txt", error) ||
        !characters.LoadWeaponSkills(directory + L"weapon-skills.txt", error) || !maps.LoadData(dataRoot, error) ||
        !june14505::LoadWorldDiscovery(directory + L"discovery.txt", discovery, error, 14422))
        return false;
    return june14505::CreateBootstrapFromCatalog(std::move(characters), std::move(maps), std::move(discovery), out,
                                                 error);
}
} // namespace june14422
} // namespace glide
