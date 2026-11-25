#pragma once
#include "character_catalog.h"
#include "glide/profile/create_flow.h"
namespace glide {
namespace hot49284 {
bool CreateCharacterFlow(CharacterCatalog catalog, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error);
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error);
} // namespace hot49284
} // namespace glide
