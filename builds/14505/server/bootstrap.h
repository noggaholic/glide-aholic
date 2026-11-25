#pragma once
#include "glide/profile/game_bootstrap.h"
#include <memory>

namespace glide {
namespace june14505 {
class CharacterCatalog;
class MapGeometryCatalog;
struct WorldDiscovery;
bool CreateBootstrapFromCatalog(CharacterCatalog characters, MapGeometryCatalog maps, WorldDiscovery discovery,
                                std::unique_ptr<GameBootstrap>& out, std::string& error);
bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out, std::string& error);
} // namespace june14505
} // namespace glide
