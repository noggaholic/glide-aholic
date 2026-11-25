#pragma once
#include "glide/profile/auth_flow.h"
#include <memory>

namespace glide {
namespace june14422 {
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error);
}
} // namespace glide
