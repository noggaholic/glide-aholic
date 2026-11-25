#pragma once
#include "../../14505/server/map_geometry.h"

namespace glide {
namespace april14075 {
class MapGeometryCatalog final : public june14505::MapGeometryCatalog {
  public:
    MapGeometryCatalog() : june14505::MapGeometryCatalog(14075) {}
    bool LoadData(const std::wstring& dataRoot, std::string& error);
};
} // namespace april14075
} // namespace glide
