#include "glide/catalog.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include "glide/kv.h"
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <sstream>
#include <vector>

namespace glide {
bool Catalog::WorldPosition(uint32_t map, uint32_t point, CatalogPosition& out) const {
    const auto found = maps.find(map);
    if (found == maps.end())
        return false;
    const auto placed = found->second.positions.find(point);
    if (placed == found->second.positions.end())
        return false;
    auto candidate = placed->second;
    if (positionSpace == CatalogPositionSpace::Native) {
        candidate.x /= 32;
        candidate.y /= 32;
        candidate.z /= -32;
    } else if (positionSpace != CatalogPositionSpace::Server)
        return false;
    if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y) || !std::isfinite(candidate.z))
        return false;
    out = candidate;
    return true;
}
bool CatalogText(const std::string& value) {
    if (value.empty())
        return false;
    for (unsigned char ch : value)
        if (ch < 32 || ch == '|' || ch == 127)
            return false;
    std::wstring wide;
    return Utf8ToWide(value, wide);
}
namespace {
std::string Trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return {};
    return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
}
std::vector<std::string> Split(const std::string& line) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const auto end = line.find('|', start);
        out.push_back(Trim(line.substr(start, end == std::string::npos ? end : end - start)));
        if (end == std::string::npos)
            return out;
        start = end + 1;
    }
}
bool Number(const std::string& value, uint32_t& out, unsigned maximum = UINT32_MAX) {
    return ParseUnsigned(value, maximum, out);
}
bool Position(const std::string& text, float& out) {
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (!locale)
        return false;
    char* end = nullptr;
    errno = 0;
    const float value = _strtof_l(text.c_str(), &end, locale);
    const bool valid = end != text.c_str() && !*end && errno != ERANGE && std::isfinite(value);
    _free_locale(locale);
    if (!valid)
        return false;
    out = value;
    return true;
}
bool TextFile(const std::wstring& path, size_t maximum, std::string& out) {
    Buffer bytes;
    if (!ReadFileBytes(path, maximum, bytes))
        return false;
    out.assign(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size());
    return out.find('\0') == std::string::npos;
}
bool HashMatches(const std::string& bytes, const std::string& expected) {
    if (expected.size() != 64)
        return false;
    uint8_t digest[32];
    if (!Sha256({bytes.data(), bytes.size()}, digest))
        return false;
    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < 32; ++i)
        if (expected[i * 2] != hex[digest[i] >> 4] || expected[i * 2 + 1] != hex[digest[i] & 15])
            return false;
    return true;
}
template <typename Parse> bool Table(const std::string& text, size_t fields, Parse parse) {
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        const auto values = Split(line);
        if (values.size() != fields || !parse(values))
            return false;
    }
    return true;
}
bool Reveal(const std::string& text, Catalog& catalog) {
    std::istringstream input(text);
    std::string line;
    unsigned stage = 0;
    CatalogMap* map = nullptr;
    std::set<uint32_t> seen;
    while (std::getline(input, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#')
            continue;
        if (!stage) {
            if (line != "format 2")
                return false;
            stage = 1;
            continue;
        }
        if (stage == 1) {
            uint32_t id = 0;
            if (line.compare(0, 4, "map ") || !Number(line.substr(4), id, UINT16_MAX) || !seen.insert(id).second)
                return false;
            const auto found = catalog.maps.find(id);
            if (found == catalog.maps.end())
                return false;
            map = &found->second;
            stage = 2;
            continue;
        }
        if (stage == 4) {
            if (line != "end")
                return false;
            stage = 1;
            continue;
        }
        const std::string prefix = stage == 2 ? "pois " : "sectors ";
        if (line.compare(0, prefix.size(), prefix))
            return false;
        const auto values = Trim(line.substr(prefix.size()));
        if (values.empty())
            return false;
        if (values != "-") {
            std::istringstream words(values);
            std::string word;
            uint32_t wordIndex = 0;
            while (words >> word) {
                if (stage == 2) {
                    uint32_t id = 0;
                    if (!Number(word, id) || !id || !map->points.insert(id).second)
                        return false;
                } else {
                    if (word.size() != 8 || wordIndex >= UINT16_MAX)
                        return false;
                    uint32_t bits = 0;
                    for (char ch : word) {
                        unsigned digit = 0;
                        if (ch >= '0' && ch <= '9')
                            digit = unsigned(ch - '0');
                        else if (ch >= 'a' && ch <= 'f')
                            digit = unsigned(ch - 'a' + 10);
                        else if (ch >= 'A' && ch <= 'F')
                            digit = unsigned(ch - 'A' + 10);
                        else
                            return false;
                        bits = (bits << 4) | digit;
                    }
                    for (unsigned bit = 0; bit < 32; ++bit)
                        if (bits & (1u << bit))
                            map->sectors.insert(wordIndex * 32 + bit);
                    ++wordIndex;
                }
            }
        }
        ++stage;
    }
    return stage == 1 && seen.size() == catalog.maps.size();
}
} // namespace
bool LoadCatalog(const std::wstring& directory, uint32_t build, Catalog& out, std::string& error) {
    auto fail = [&](const std::string& reason) {
        error = "Catalog " + std::to_string(build) + ": " + reason + ". Run glide-catalog.exe for this build";
        return false;
    };
    std::string manifestText, detail, value;
    KvDocument manifest;
    uint32_t format = 0, storedBuild = 0;
    if (!build || !TextFile(directory + L"/catalog.cfg", 65536, manifestText) ||
        !KvDocument::Parse(manifestText, manifest, detail) || !manifest.Get("", "format", value) ||
        !Number(value, format) || format != 2 || !manifest.Get("", "build", value) || !Number(value, storedBuild) ||
        storedBuild != build)
        return fail("missing, invalid or mismatched manifest");
    const char* names[] = {"maps.txt", "map-files.txt", "map-reveal.txt", "waypoints.txt", "gliders.txt", "mounts.txt"};
    std::map<std::string, std::string> files;
    for (const auto name : names) {
        std::wstring wide;
        std::string text;
        if (!Utf8ToWide(name, wide) || !manifest.Get("", "sha256." + std::string(name), value) ||
            !TextFile(directory + L"/" + wide, 32u * 1024u * 1024u, text) || !HashMatches(text, value))
            return fail(std::string("missing file or hash mismatch: ") + name);
        files.emplace(name, std::move(text));
    }
    if (manifest.Get("", "sha256.waypoint-travel.txt", value)) {
        std::string text;
        if (!TextFile(directory + L"/waypoint-travel.txt", 32u * 1024u * 1024u, text) || !HashMatches(text, value))
            return fail("missing file or hash mismatch: waypoint-travel.txt");
        files.emplace("waypoint-travel.txt", std::move(text));
    }
    Catalog candidate;
    candidate.build = build;
    // Before this field existed, the two modern extractors wrote server units;
    // beta catalogs wrote native units. Preserve those published contracts.
    candidate.positionSpace =
        build == 204489 || build == 167409 ? CatalogPositionSpace::Server : CatalogPositionSpace::Native;
    if (manifest.Get("", "position-space", value)) {
        if (value == "server")
            candidate.positionSpace = CatalogPositionSpace::Server;
        else if (value == "native")
            candidate.positionSpace = CatalogPositionSpace::Native;
        else
            return fail("invalid position-space");
    }
    if (!Table(files["maps.txt"], 6,
               [&](const std::vector<std::string>& fields) {
                   CatalogMap map;
                   uint32_t available = 0;
                   if (!Number(fields[0], map.id, UINT16_MAX) || !map.id || !Number(fields[1], map.type, UINT16_MAX) ||
                       !CatalogText(fields[2]) || !CatalogText(fields[3]) || !CatalogText(fields[4]) ||
                       !Number(fields[5], available, 1))
                       return false;
                   map.name = fields[3];
                   map.token = fields[4];
                   map.hasFile = available != 0;
                   return candidate.maps.emplace(map.id, std::move(map)).second;
               }) ||
        candidate.maps.empty())
        return fail("invalid maps.txt");
    std::set<uint32_t> mapped;
    if (!Table(files["map-files.txt"], 4,
               [&](const std::vector<std::string>& fields) {
                   uint32_t id = 0, file = 0, mft = 0, available = 0;
                   if (!Number(fields[0], id, UINT16_MAX) || !mapped.insert(id).second || !Number(fields[1], file) ||
                       !file || !Number(fields[2], mft) || !Number(fields[3], available, 1) || (available && !mft))
                       return false;
                   const auto found = candidate.maps.find(id);
                   if (found == candidate.maps.end() || found->second.hasFile != (available != 0))
                       return false;
                   found->second.file = file;
                   found->second.mft = mft;
                   return true;
               }) ||
        mapped.size() != candidate.maps.size())
        return fail("invalid map-files.txt");
    if (!Reveal(files["map-reveal.txt"], candidate))
        return fail("invalid map-reveal.txt");
    if (!Table(files["waypoints.txt"], 5, [&](const std::vector<std::string>& fields) {
            uint32_t id = 0, map = 0;
            CatalogPosition position;
            if (!Number(fields[0], id) || !id || !Number(fields[1], map, UINT16_MAX) ||
                !Position(fields[2], position.x) || !Position(fields[3], position.y) ||
                !Position(fields[4], position.z))
                return false;
            const auto found = candidate.maps.find(map);
            return found != candidate.maps.end() && found->second.points.count(id) &&
                   found->second.positions.emplace(id, position).second;
        }))
        return fail("invalid waypoints.txt");
    if (!Table(files["waypoint-travel.txt"], 3, [&](const std::vector<std::string>& fields) {
            uint32_t id = 0, restricted = 0;
            CatalogWaypoint waypoint;
            if (!Number(fields[0], id) || !id || !Number(fields[1], waypoint.map, UINT16_MAX) ||
                !Number(fields[2], restricted, 1))
                return false;
            const auto found = candidate.maps.find(waypoint.map);
            if (found == candidate.maps.end() || !found->second.positions.count(id))
                return false;
            waypoint.noCrossMap = restricted != 0;
            return candidate.waypoints.emplace(id, waypoint).second;
        }))
        return fail("invalid waypoint-travel.txt");
    if (!Table(files["gliders.txt"], 2, [&](const std::vector<std::string>& fields) {
            CatalogDesign design;
            if (!Number(fields[0], design.id) || !design.id || !CatalogText(fields[1]))
                return false;
            design.name = fields[1];
            return candidate.gliders.emplace(design.id, std::move(design)).second;
        }))
        return fail("invalid gliders.txt");
    if (!Table(files["mounts.txt"], 6, [&](const std::vector<std::string>& fields) {
            CatalogDesign design;
            if (!Number(fields[0], design.id) || !design.id || !CatalogText(fields[1]) ||
                !Number(fields[2], design.mount) || !Number(fields[3], design.family) || !CatalogText(fields[4]) ||
                !Number(fields[5], design.content) || !design.content)
                return false;
            design.name = fields[1];
            return candidate.mounts.emplace(design.id, std::move(design)).second;
        }))
        return fail("invalid mounts.txt");
    out = std::move(candidate);
    error.clear();
    return true;
}
} // namespace glide
