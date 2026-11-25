#pragma once
#include <array>
#include <cstdint>
#include "glide/files.h"
#include <cmath>
#include <cstring>
#include <vector>
namespace glide {
namespace july14812 {
struct WorldMapGeometry {
    uint32_t map, file, mft;
    std::array<float, 4> bounds;
    // Local choice of an original portal preload; not a recovered server spawn.
    std::array<float, 3> localArrival;
};
class WorldMapCatalog {
  public:
    bool Load(const std::wstring& file, std::string& error) {
        Buffer bytes;
        error = "Invalid local July map geometry";
        if (!ReadFileBytes(file, 4 * 1024 * 1024, bytes) || !bytes.Size() || bytes.Size() % 40)
            return false;
        Reader reader(bytes.View());
        std::vector<WorldMapGeometry> maps;
        const auto number = [&](float& value) {
            uint32_t bits = 0;
            if (!reader.U32(bits))
                return false;
            std::memcpy(&value, &bits, sizeof(value));
            return std::isfinite(value);
        };
        while (reader.Remaining()) {
            WorldMapGeometry row{};
            if (!reader.U32(row.map) || !reader.U32(row.file) || !reader.U32(row.mft) || !row.map || row.map > 65535 ||
                !row.file || !row.mft || (!maps.empty() && maps.back().map >= row.map))
                return false;
            for (auto& value : row.bounds)
                if (!number(value))
                    return false;
            for (auto& value : row.localArrival)
                if (!number(value))
                    return false;
            if (row.bounds[0] >= row.bounds[2] || row.bounds[1] >= row.bounds[3])
                return false;
            maps.push_back(row);
        }
        maps_ = std::move(maps);
        error.clear();
        return true;
    }
    const WorldMapGeometry* Find(uint32_t map) const {
        for (const auto& row : maps_)
            if (row.map == map)
                return &row;
        return nullptr;
    }

  private:
    std::vector<WorldMapGeometry> maps_;
};
} // namespace july14812
} // namespace glide
