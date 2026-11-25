#pragma once
#include "glide/buffer.h"
#include <set>
#include <string>

namespace glide {
namespace june14505 {
// All published definitions, including those outside the current map.
struct WorldDiscovery {
    std::set<uint32_t> points, sectors;
    uint32_t build = 14505;
};
bool ParseWorldDiscovery(const std::string& text, WorldDiscovery& out, std::string& error, uint32_t build = 14505);
bool LoadWorldDiscovery(const std::wstring& path, WorldDiscovery& out, std::string& error, uint32_t build = 14505);
bool DiscoveryMessage(const WorldDiscovery& discovery, Buffer& out);
bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out);
bool TimeOfDayForBuild(unsigned build, uint32_t tick, float rate, bool paused, Buffer& out);
} // namespace june14505
} // namespace glide
