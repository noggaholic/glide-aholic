#include "create.h"
#include "character_catalog.h"
#include "../../14505/server/create.h"

namespace glide {
namespace april14075 {
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.LoadData(dataRoot, error))
        return false;
    return june14505::CreateCharacterFlowFromCatalog(std::move(catalog), initial, out, error);
}
} // namespace april14075
} // namespace glide
