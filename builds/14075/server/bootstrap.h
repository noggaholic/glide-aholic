#pragma once
#include "entry_steps.h"
#include <memory>

namespace glide {
namespace april14075 {
bool CreateBootstrap(CharacterCatalog characters, MapGeometryCatalog maps, std::unique_ptr<GameBootstrap>& out,
                     std::string& error);
bool LoadBootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>& out, std::string& error);
} // namespace april14075
} // namespace glide
