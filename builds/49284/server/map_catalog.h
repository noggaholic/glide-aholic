#pragma once
#include "glide/buffer.h"
#include <array>
#include <map>
#include <string>
namespace glide {
namespace hot49284 {
struct MapAsset {
    uint32_t id = 0, revision = 0, size = 0, flags = 0;
};
struct MapGeometry {
    uint16_t id = 0;
    bool portalPosition = false, previouslyNative = false;
    std::array<uint8_t, 16> guid{};
    std::array<MapAsset, 2> assets{};
    std::array<float, 3> position{};
    std::array<float, 4> bounds{};
};
class MapCatalog {
  public:
    bool Load(const std::wstring& path, std::string& error);
    bool Parse(ByteView, std::string& error);
    const MapGeometry* Find(unsigned id) const;
    const MapGeometry* FindGuid(const std::array<uint8_t, 16>& guid) const;
    size_t Count() const {
        return maps_.size();
    }

  private:
    std::map<unsigned, MapGeometry> maps_;
};
} // namespace hot49284
} // namespace glide
