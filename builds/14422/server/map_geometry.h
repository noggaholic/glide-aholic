#pragma once
#include "../../14505/server/map_geometry.h"

namespace glide {
namespace june14422 {
class MapGeometryCatalog final : public june14505::MapGeometryCatalog {
  public:
    MapGeometryCatalog() : june14505::MapGeometryCatalog(14422) {}
    bool LoadData(const std::wstring& dataRoot, std::string& error);
};
} // namespace june14422
} // namespace glide
