#include "map_catalog.h"
#include "glide/files.h"
#include <cmath>
#include <cstring>
#include <set>
#include <utility>
namespace glide {
namespace hot49284 {
const MapGeometry* MapCatalog::Find(unsigned id) const {
    const auto found = maps_.find(id);
    return found == maps_.end() ? nullptr : &found->second;
}
const MapGeometry* MapCatalog::FindGuid(const std::array<uint8_t, 16>& guid) const {
    for (const auto& entry : maps_)
        if (entry.second.guid == guid)
            return &entry.second;
    return nullptr;
}
bool MapCatalog::Load(const std::wstring& path, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 16 + 80 * 65535, bytes)) {
        error = "Cannot read HoT maps";
        return false;
    }
    return Parse(bytes.View(), error);
}
bool MapCatalog::Parse(ByteView bytes, std::string& error) {
    error = "Invalid HoT map catalog";
    if (!bytes.Valid() || bytes.size < 16 || std::memcmp(bytes.data, "HOTMAP01", 8))
        return false;
    Reader reader(bytes);
    ByteView magic;
    uint32_t build, count;
    if (!reader.Bytes(8, magic) || !reader.U32(build) || build != 49284 || !reader.U32(count) || !count ||
        count > 65535 || bytes.size != 16 + 80 * static_cast<size_t>(count))
        return false;
    MapCatalog result;
    std::set<std::array<uint8_t, 16>> guids;
    uint16_t previous = 0;
    for (uint32_t index = 0; index < count; ++index) {
        MapGeometry map;
        uint8_t origin, verified;
        ByteView guid;
        if (!reader.U16(map.id) || map.id <= previous || !reader.U8(origin) || origin > 1 || !reader.U8(verified) ||
            verified > 1 || !reader.Bytes(16, guid))
            return false;
        map.portalPosition = origin != 0;
        map.previouslyNative = verified != 0;
        std::memcpy(map.guid.data(), guid.data, 16);
        if (map.guid == std::array<uint8_t, 16>{} || !guids.insert(map.guid).second)
            return false;
        for (auto& asset : map.assets) {
            if (!reader.U32(asset.id) || !reader.U32(asset.revision) || !reader.U32(asset.size) ||
                !reader.U32(asset.flags))
                return false;
            if (asset.id ? (!asset.revision || !asset.size) : (asset.revision || asset.size || asset.flags))
                return false;
        }
        if (!map.assets[0].id || (map.portalPosition && !map.assets[1].id))
            return false;
        const auto number = [&](float& value) {
            uint32_t bits;
            if (!reader.U32(bits))
                return false;
            std::memcpy(&value, &bits, 4);
            return std::isfinite(value);
        };
        for (float& value : map.position)
            if (!number(value))
                return false;
        for (float& value : map.bounds)
            if (!number(value))
                return false;
        const auto& p = map.position;
        const auto& b = map.bounds;
        if (b[0] >= b[2] || b[1] >= b[3] || p[0] < b[0] || p[0] > b[2] || p[1] < b[1] || p[1] > b[3])
            return false;
        previous = map.id;
        result.maps_.emplace(map.id, std::move(map));
    }
    maps_ = std::move(result.maps_);
    error.clear();
    return true;
}
} // namespace hot49284
} // namespace glide
