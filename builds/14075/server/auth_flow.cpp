#include "auth_flow.h"
#include "character_catalog.h"
#include "../../14505/server/auth_flow.h"

namespace glide {
namespace april14075 {
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.LoadData(dataRoot, error))
        return false;
    return june14505::CreateBetaAuthFlowFromCatalog(std::move(catalog), out, error);
}
} // namespace april14075
} // namespace glide
