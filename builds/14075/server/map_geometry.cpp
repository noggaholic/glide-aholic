#include "map_geometry.h"

namespace glide {
namespace april14075 {
bool MapGeometryCatalog::LoadData(const std::wstring& dataRoot, std::string& error) {
    return Load(dataRoot + L"/14075/map-geometry.txt", error);
}
} // namespace april14075
} // namespace glide
