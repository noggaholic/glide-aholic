#pragma once
#include <array>
#include <map>
#include <string>

namespace glide {
namespace june14505 {
struct MapGeometry {
    unsigned id = 0, uid = 0, fileClient = 0, archive = 0, type = 0;
    std::array<float, 4> bounds{};
};
class MapGeometryCatalog {
  public:
    explicit MapGeometryCatalog(unsigned build = 14505) : build_(build) {}
    unsigned Build() const {
        return build_;
    }
    bool Load(const std::wstring& path, std::string& error);
    bool Parse(const std::string& text, std::string& error);
    const MapGeometry* Find(unsigned id) const;
    size_t Count() const {
        return maps_.size();
    }

  private:
    unsigned build_;
    std::map<unsigned, MapGeometry> maps_;
};
} // namespace june14505
} // namespace glide
