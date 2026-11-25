#pragma once
#include "character_catalog.h"
#include "glide/profile/auth_flow.h"
namespace glide {
namespace july14812 {
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>&, std::string& error);
bool CreateAuthFlow(CharacterCatalog, const std::array<uint8_t, 32>& contentKey, std::unique_ptr<glide::AuthFlow>&,
                    std::string& error);
} // namespace july14812
} // namespace glide
