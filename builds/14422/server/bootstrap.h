#pragma once
#include "glide/profile/game_bootstrap.h"
#include <memory>

namespace glide {
namespace june14422 {
bool LoadBootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error);
}
} // namespace glide
