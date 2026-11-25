#include "auth_flow.h"
#include "character_catalog.h"
#include "../../14505/server/auth_flow.h"

namespace glide {
namespace june14422 {
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.LoadData(dataRoot, error) || !catalog.LoadWeapons(dataRoot + L"/14422/weapon-loadouts.txt", error))
        return false;
    return june14505::CreateAuthFlowFromCatalog(std::move(catalog), out, error);
}
} // namespace june14422
} // namespace glide
