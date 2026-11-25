#pragma once
#include "character_catalog.h"
#include "glide/profile/auth_flow.h"
#include <functional>
namespace glide {
namespace hot49284 {
// Clock injection keeps epoch conversion testable; production uses Unix seconds.
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error);
bool CreateAuthFlow(CharacterCatalog catalog, std::function<uint64_t()> clock, std::unique_ptr<glide::AuthFlow>& out,
                    std::string& error);
} // namespace hot49284
} // namespace glide
