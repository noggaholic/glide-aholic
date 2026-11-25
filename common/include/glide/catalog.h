#pragma once
#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace glide {
struct CatalogPosition {
    float x = 0, y = 0, z = 0;
};
enum class CatalogPositionSpace { Native, Server };
struct CatalogMap {
    uint32_t id = 0, type = 0, file = 0, mft = 0;
    bool hasFile = false;
    std::string name, token;
    std::set<uint32_t> points, sectors;
    std::map<uint32_t, CatalogPosition> positions;
};
struct CatalogWaypoint {
    uint32_t map = 0;
    bool noCrossMap = false;
};
struct CatalogDesign {
    uint32_t id = 0, text = 0, mount = 0, family = 0;
    uint32_t content = 0; // Internal MountSkinDef reference, distinct from public skin id.
    std::string name;
};
struct Catalog {
    uint32_t build = 0;
    CatalogPositionSpace positionSpace = CatalogPositionSpace::Native;
    std::map<uint32_t, CatalogMap> maps;
    std::map<uint32_t, CatalogDesign> gliders, mounts;
    // Only verified travel shrines; positions alone also contain ordinary POIs.
    std::map<uint32_t, CatalogWaypoint> waypoints;
    // Convert archive positions once; modern catalogs already use server units.
    bool WorldPosition(uint32_t map, uint32_t point, CatalogPosition& out) const;
    bool Available(uint32_t map) const {
        const auto found = maps.find(map);
        return found != maps.end() && found->second.hasFile && found->second.type != 1;
    }
};
bool CatalogText(const std::string& value);
// Requires the manifest's build and six base hashes, plus travel metadata when declared.
// Replaces output only on success; older catalogs have no verified travel shrines.
bool LoadCatalog(const std::wstring& directory, uint32_t build, Catalog& out, std::string& error);
} // namespace glide
