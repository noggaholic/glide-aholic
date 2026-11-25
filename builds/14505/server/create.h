#pragma once
#include "glide/profile/create_flow.h"
#include <memory>

namespace glide {
namespace june14505 {
class CharacterCatalog;
bool CreateCharacterFlowFromCatalog(CharacterCatalog catalog, const SpawnPoint& initial,
                                    std::unique_ptr<CreateFlow>& out, std::string& error);
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error);
} // namespace june14505
} // namespace glide
