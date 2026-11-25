#include "map_geometry.h"
#include "glide/files.h"
#include "glide/kv.h"
#include <sstream>
#include <cmath>

namespace glide {
namespace june14505 {
const MapGeometry* MapGeometryCatalog::Find(unsigned id) const {
    const auto at = maps_.find(id);
    return at == maps_.end() ? nullptr : &at->second;
}
bool MapGeometryCatalog::Load(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read June map geometry";
        return false;
    }
    return Parse(std::string(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size()), error);
}
bool MapGeometryCatalog::Parse(const std::string& text, std::string& error) {
    error = "Invalid June map geometry";
    if ((build_ != 14075 && build_ != 14422 && build_ != 14505) || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    MapGeometryCatalog result(build_);
    std::istringstream input(text);
    std::string line;
    bool build = false;
    while (std::getline(input, line)) {
        if (line.size() > 512)
            return false;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#')
            continue;
        std::istringstream row(line);
        row.imbue(std::locale::classic());
        std::string tag, extra;
        if (!(row >> tag))
            return false;
        if (!build) {
            if (tag != "build" || !(row >> extra) || extra != std::to_string(build_) || (row >> extra))
                return false;
            build = true;
            continue;
        }
        if (tag != "map")
            return false;
        unsigned values[5];
        for (size_t i = 0; i < 5; ++i) {
            if (!(row >> extra) || !ParseUnsigned(extra, i == 0 ? UINT16_MAX : UINT32_MAX, values[i]))
                return false;
        }
        MapGeometry map;
        map.id = values[0];
        map.uid = values[1];
        map.fileClient = values[2];
        map.archive = values[3];
        map.type = values[4];
        if (!map.id || !map.uid ||
            (map.type != 0 && map.type != 1 && map.type != 2 && map.type != 4 && map.type != 5 && map.type != 7 &&
             map.type != 9 && map.type != 10 && map.type != 11 && map.type != 12))
            return false;
        for (float& value : map.bounds)
            if (!(row >> value) || !std::isfinite(value))
                return false;
        if ((row >> extra) || map.bounds[0] >= map.bounds[2] || map.bounds[1] >= map.bounds[3] ||
            !result.maps_.emplace(map.id, map).second)
            return false;
    }
    if (!build || result.maps_.empty())
        return false;
    *this = std::move(result);
    error.clear();
    return true;
}
} // namespace june14505
} // namespace glide
