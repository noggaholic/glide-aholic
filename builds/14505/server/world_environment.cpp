#include "world_environment.h"
#include "world_protocol.h"
#include <array>
#include <cmath>
#include <sstream>
#include "glide/files.h"
#include "glide/kv.h"

namespace glide {
namespace june14505 {
bool ParseWorldDiscovery(const std::string& text, WorldDiscovery& out, std::string& error, uint32_t expectedBuild) {
    error = "Invalid June discovery catalog";
    if ((expectedBuild != 14422 && expectedBuild != 14505) || text.size() > 1024 * 1024 ||
        text.find('\0') != std::string::npos)
        return false;
    WorldDiscovery result;
    result.build = expectedBuild;
    std::istringstream input(text);
    std::string line;
    bool build = false;
    while (std::getline(input, line)) {
        if (line.size() > 256)
            return false;
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos || line[first] == '#')
            continue;
        std::istringstream row(line);
        std::string tag, value, extra;
        if (!(row >> tag >> value) || (row >> extra))
            return false;
        if (!build) {
            if (tag != "build" || value != std::to_string(expectedBuild))
                return false;
            build = true;
            continue;
        }
        unsigned id;
        if (!ParseUnsigned(value, 3999, id))
            return false;
        if (tag == "point") {
            if (!result.points.insert(id).second)
                return false;
        } else if (tag == "sector") {
            if (!result.sectors.insert(id).second)
                return false;
        } else
            return false;
    }
    if (!build || result.points.empty() || result.sectors.empty())
        return false;
    out = std::move(result);
    error.clear();
    return true;
}
bool LoadWorldDiscovery(const std::wstring& path, WorldDiscovery& out, std::string& error, uint32_t build) {
    Buffer bytes;
    if (!ReadFileBytes(path, 1024 * 1024, bytes)) {
        error = "Cannot read June discovery catalog";
        return false;
    }
    return ParseWorldDiscovery(
        std::string(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size()), out, error, build);
}
bool DiscoveryMessage(const WorldDiscovery& discovery, Buffer& out) {
    if (discovery.build != 14422 && discovery.build != 14505)
        return false;
    Buffer result;
    const std::set<uint32_t>* groups[] = {&discovery.points, &discovery.sectors};
    const uint16_t messages[] = {0x021f, 0x024a};
    for (size_t group = 0; group < 2; ++group) {
        std::array<uint32_t, 125> words{};
        size_t count = 1;
        for (uint32_t id : *groups[group]) {
            const size_t word = id / 32;
            if (word >= words.size())
                return false;
            words[word] |= 1u << (id % 32);
            if (count <= word)
                count = word + 1;
        }
        if (!result.U16(messages[group]) || !result.U16(static_cast<uint16_t>(count)))
            return false;
        for (size_t i = 0; i < count; ++i)
            if (!result.VarU32(words[i]))
                return false;
    }
    out = std::move(result);
    return true;
}
bool TimeOfDayForBuild(unsigned build, uint32_t tick, float rate, bool paused, Buffer& out) {
    if (!WorldProtocolBuild(build) || tick >= 7200000 || !std::isfinite(rate) || rate < 0)
        return false;
    Buffer result;
    if (!result.U16(build == 14075 ? 0x01e4 : 0x024d) || !result.VarU32(tick) || !result.F32(paused ? 0 : rate))
        return false;
    out = std::move(result);
    return true;
}
bool TimeOfDay(uint32_t tick, float rate, bool paused, Buffer& out) {
    return TimeOfDayForBuild(14505, tick, rate, paused, out);
}
} // namespace june14505
} // namespace glide
