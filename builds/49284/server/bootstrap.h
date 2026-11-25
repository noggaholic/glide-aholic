#pragma once
#include "entry_steps.h"
#include <memory>
namespace glide {
namespace hot49284 {
bool CreateBootstrap(CharacterCatalog, MapCatalog, std::unique_ptr<GameBootstrap>& out, std::string& error);
bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out, std::string& error);
} // namespace hot49284
} // namespace glide
