#include "map_geometry.h"

namespace glide {
namespace june14422 {
bool MapGeometryCatalog::LoadData(const std::wstring& dataRoot, std::string& error) {
    return Load(dataRoot + L"/14422/map-geometry.txt", error);
}
} // namespace june14422
} // namespace glide
