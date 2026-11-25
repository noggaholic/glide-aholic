#pragma once
#include "glide/profile/auth_flow.h"
#include <memory>

namespace glide {
namespace june14505 {
class CharacterCatalog;
// Only the native-audited June profiles share this wire flow; catalogs remain build-specific.
bool CreateAuthFlowFromCatalog(CharacterCatalog catalog, std::unique_ptr<glide::AuthFlow>& out, std::string& error);
// Explicit beta entry point; callers must load the requested build's catalog first.
bool CreateBetaAuthFlowFromCatalog(CharacterCatalog catalog, std::unique_ptr<glide::AuthFlow>& out, std::string& error);
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error);
} // namespace june14505
} // namespace glide
