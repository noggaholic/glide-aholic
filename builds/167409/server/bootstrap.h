#pragma once
#include "glide/profile/game_bootstrap.h"
#include <memory>
namespace glide {
const GameBootstrap& China167409Bootstrap();
bool LoadChina167409Bootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error);
} // namespace glide
