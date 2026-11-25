#include "create.h"
#include "character_catalog.h"
#include "../../14505/server/create.h"

namespace glide {
namespace june14422 {
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.LoadData(dataRoot, error) || !catalog.LoadWeapons(dataRoot + L"/14422/weapon-loadouts.txt", error))
        return false;
    return june14505::CreateCharacterFlowFromCatalog(std::move(catalog), initial, out, error);
}
} // namespace june14422
} // namespace glide
