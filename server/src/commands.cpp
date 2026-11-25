#include "commands.h"
#include "glide/config.h"
#include "glide/profile/combat_traits.h"
#include "glide/profile/dragon_facet_data.h"
#include "glide/files.h"
#include <cstdio>
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace glide {
namespace {
std::string Trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos)
        return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1);
}
std::string Lower(std::string text) {
    for (char& c : text)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return text;
}
bool Gps(const CommandContext& context, const std::string& args, CommandResult& result) {
    auto& replies = result.lines;
    if (!args.empty())
        return false;
    char line[192];
    std::snprintf(line, sizeof(line), "Map ID: %u | X: %.3f | Y: %.3f | Z: %.3f", context.character.map,
                  context.position.x, context.position.y, context.position.z);
    replies.emplace_back(line);
    return true;
}
bool Degrees(const Facing* facing, double& degrees) {
    if (!facing || !std::isfinite(facing->x) || !std::isfinite(facing->y) || !std::isfinite(facing->z) ||
        std::hypot(static_cast<double>(facing->x), static_cast<double>(facing->y)) <=
            std::numeric_limits<float>::epsilon())
        return false;
    degrees =
        std::atan2(static_cast<double>(facing->y), static_cast<double>(facing->x)) * (180.0 / 3.14159265358979323846);
    if (degrees < 0)
        degrees += 360;
    return true;
}
bool Heading(const CommandContext& context, const std::string& args, CommandResult& result) {
    auto& replies = result.lines;
    if (!args.empty())
        return false;
    double degrees;
    if (!Degrees(context.facing, degrees)) {
        replies.emplace_back("Heading is not available yet. Move or turn, then try again.");
        return true;
    }
    char line[224];
    std::snprintf(line, sizeof(line), "Heading: %.3f degrees | X: %.3f | Y: %.3f | Z: %.3f", degrees, context.facing->x,
                  context.facing->y, context.facing->z);
    replies.emplace_back(line);
    return true;
}
bool Status(const CommandContext& context, const std::string& args, CommandResult& result) {
    auto& replies = result.lines;
    if (!args.empty())
        return false;
    double degrees;
    char heading[32] = "n/a";
    if (Degrees(context.facing, degrees))
        std::snprintf(heading, sizeof(heading), "%.1f", degrees);
    const uint32_t ticks = context.timeTicks % 7200000;
    const unsigned hour = ticks / 300000, minute = (ticks % 300000) / 5000, second = ((ticks % 5000) * 60) / 5000;
    char line[384];
    std::snprintf(line, sizeof(line), "Status | Build %u | Map %u | X %.3f Y %.3f Z %.3f | Heading %s",
                  context.catalog.build, context.character.map, context.position.x, context.position.y,
                  context.position.z, heading);
    std::string status(line);
    if (context.capabilities.Has(Capability::Glider))
        status += " | Glider " + std::to_string(context.character.glider);
    if (context.capabilities.Has(Capability::Mounts))
        status += " | Mount " + std::to_string(context.mountSkin);
    std::snprintf(line, sizeof(line), " | Time %02u:%02u:%02u | Online %u", hour, minute, second,
                  static_cast<unsigned>(context.online));
    replies.emplace_back(status + line);
    return true;
}
bool Trait(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (!args.empty() && args != "1453" && args != "off")
        return false;
    if (!combat::CanSelectCombatTrait(context.catalog.build, context.character)) {
        result.lines.emplace_back("This command requires a Warrior.");
        return true;
    }
    unsigned selected = 0;
    if (!combat::ReadCombatTrait(context.catalog.build, context.character, selected))
        return false;
    if (!args.empty()) {
        selected = args == "off" ? 0 : combat::kPinnacleOfStrength;
        result.changeTrait = true;
    }
    result.combatTrait = selected;
    result.lines.emplace_back(selected ? "Pinnacle of Strength (1453): selected for Might power. Critical chance and "
                                         "the trait panel are not connected yet."
                                       : "Combat trait: off.");
    return true;
}
bool Combat(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (!args.empty() && args != "on" && args != "off")
        return false;
    const bool enabled = args.empty() ? context.combatEnabled : args == "on";
    result.changeCombat = !args.empty();
    result.combatEnabled = enabled;
    result.lines.emplace_back(enabled
                                  ? "Combat enabled against other participants on this map. It resets when you leave."
                                  : "Combat disabled. Other players remain allies.");
    if (enabled)
        for (const auto& player : context.players)
            if (player.combatEnabled && player.map == context.character.map && player.name != context.character.name)
                result.lines.emplace_back("Opponent: " + player.name);
    return true;
}
bool TimeOfDay(const CommandContext& context, const std::string& args, CommandResult& result) {
    unsigned hour, minute, second;
    if (args.empty()) {
        const uint32_t ticks = context.timeTicks % 7200000;
        hour = ticks / 300000;
        minute = (ticks % 300000) / 5000;
        second = ((ticks % 5000) * 60) / 5000;
    } else {
        unsigned fields[3] = {};
        size_t index = 0, digits = 0;
        for (char c : args) {
            if (c == ':' && digits && index < 2) {
                ++index;
                digits = 0;
            } else if (c >= '0' && c <= '9' && digits < 2) {
                fields[index] = fields[index] * 10 + (c - '0');
                ++digits;
            } else
                return false;
        }
        if (!digits || index < 1)
            return false;
        hour = fields[0];
        minute = fields[1];
        second = fields[2];
        if (hour > 23 || minute > 59 || second > 59) {
            result.lines.emplace_back("Time must use 00-23 hours and 00-59 minutes/seconds.");
            return true;
        }
        result.changeTime = true;
        result.timeTicks = hour * 300000 + minute * 5000 + (second * 5000 + 30) / 60;
    }
    char line[128];
    std::snprintf(line, sizeof(line),
                  result.changeTime ? "Time of day set to %02u:%02u:%02u for all players."
                                    : "Time of day: %02u:%02u:%02u",
                  hour, minute, second);
    result.lines.emplace_back(line);
    return true;
}
bool MapInfo(const CommandContext& context, const std::string& args, CommandResult& result) {
    auto& replies = result.lines;
    unsigned id = context.character.map;
    if (!args.empty() && (!ParseUnsigned(args, 65535, id) || !id))
        return false;
    const auto found = context.catalog.maps.find(id);
    if (found == context.catalog.maps.end() || !context.catalog.Available(id)) {
        replies.emplace_back("That map is not a playable entry in the local catalog.");
        return true;
    }
    const auto& map = found->second;
    replies.emplace_back("Map " + std::to_string(id) + " | Type " + std::to_string(map.type) + " | File " +
                         std::to_string(map.file) + " | Points: " + std::to_string(map.points.size()) +
                         " | Sectors: " + std::to_string(map.sectors.size()));
    if (!map.name.empty())
        replies.emplace_back(map.name);
    return true;
}
bool ContainsName(const std::string& text, const std::string& query) {
    std::wstring name, filter;
    if (!Utf8ToWide(text, name) || !Utf8ToWide(query, filter) || filter.size() > name.size())
        return false;
    for (size_t i = 0; i + filter.size() <= name.size(); ++i)
        if (CompareStringOrdinal(name.data() + i, static_cast<int>(filter.size()), filter.data(),
                                 static_cast<int>(filter.size()), TRUE) == CSTR_EQUAL)
            return true;
    return false;
}
bool Maps(const CommandContext& context, const std::string& args, CommandResult& result) {
    std::istringstream input(args);
    std::string field, query;
    unsigned page = 1;
    while (input >> field) {
        if (field == "--page") {
            if (!(input >> field) || !ParseUnsigned(field, UINT32_MAX, page) || !page || (input >> field))
                return false;
            break;
        }
        if (field.compare(0, 2, "--") == 0)
            return false;
        if (!query.empty())
            query += ' ';
        query += field;
    }
    unsigned requested = 0;
    const bool numeric = !query.empty() && query.find_first_not_of("0123456789") == std::string::npos;
    const bool validId = numeric && ParseUnsigned(query, UINT32_MAX, requested);
    std::vector<const CatalogMap*> matches;
    for (const auto& entry : context.catalog.maps) {
        const auto& map = entry.second;
        if (!context.catalog.Available(entry.first))
            continue;
        if (!query.empty() && (numeric ? (!validId || entry.first != requested) : !ContainsName(map.name, query)))
            continue;
        matches.push_back(&map);
    }
    if (matches.empty()) {
        result.lines.emplace_back("No available maps match this search.");
        return true;
    }
    const size_t pages = (matches.size() + 9) / 10;
    if (page > pages) {
        result.lines.emplace_back("Page out of range. Available pages: 1-" + std::to_string(pages) + ".");
        return true;
    }
    result.lines.emplace_back("Maps: " + std::to_string(matches.size()) + " | Page " + std::to_string(page) + "/" +
                              std::to_string(pages));
    const size_t start = (static_cast<size_t>(page) - 1) * 10;
    for (size_t i = start; i < (std::min)(start + 10, matches.size()); ++i) {
        const auto& map = *matches[i];
        result.lines.emplace_back(std::to_string(map.id) + " | " + (map.name.empty() ? "(unnamed)" : map.name));
    }
    return true;
}
bool Distance(const std::string& args, double& distance) {
    distance = 100;
    if (args.empty())
        return true;
    std::istringstream input(args);
    input.imbue(std::locale::classic());
    input >> std::noskipws >> distance;
    return !input.fail() && input.eof() && std::isfinite(distance);
}
bool Destination(double x, double y, double z, CommandResult& result) {
    const double maximum = static_cast<double>(std::numeric_limits<float>::max()) / 32;
    if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) || std::abs(x) > maximum || std::abs(y) > maximum ||
        std::abs(z) > maximum) {
        result.lines.emplace_back("The requested destination is outside the supported coordinate range.");
        return true;
    }
    result.destination = {static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)};
    result.teleport = true;
    return true;
}
bool Up(const CommandContext& context, const std::string& args, CommandResult& result) {
    double distance;
    if (!Distance(args, distance))
        return false;
    return Destination(context.position.x, context.position.y, context.position.z + distance, result);
}
bool Along(const CommandContext& context, const std::string& args, double sign, CommandResult& result) {
    double distance, degrees;
    if (!Distance(args, distance))
        return false;
    if (!Degrees(context.facing, degrees)) {
        result.lines.emplace_back("Move or turn the character, then retry the command.");
        return true;
    }
    const double length = std::hypot(static_cast<double>(context.facing->x), static_cast<double>(context.facing->y));
    return Destination(context.position.x + sign * distance * context.facing->x / length,
                       context.position.y + sign * distance * context.facing->y / length, context.position.z, result);
}
bool Forward(const CommandContext& context, const std::string& args, CommandResult& result) {
    return Along(context, args, 1, result);
}
bool Backward(const CommandContext& context, const std::string& args, CommandResult& result) {
    return Along(context, args, -1, result);
}
bool MapArrival(const CommandContext& context, unsigned map, double& x, double& y, double& z, CommandResult& result) {
    const bool transfer = map != context.character.map;
    // Reuse this build's character start if no permitted waypoint is found.
    if (context.creationSpawn.map == map) {
        x = context.creationSpawn.x;
        y = context.creationSpawn.y;
        z = context.creationSpawn.z;
    }
    const auto& destinationMap = context.catalog.maps.at(map);
    double nearest = std::numeric_limits<double>::max();
    const auto chooseWaypoints = [&](bool sharedScene) {
        for (const auto& entry : context.catalog.waypoints) {
            const auto& waypoint = entry.second;
            if (sharedScene) {
                const auto source = context.catalog.maps.find(waypoint.map);
                if (!destinationMap.mft || source == context.catalog.maps.end() ||
                    source->second.mft != destinationMap.mft || !context.catalog.Available(waypoint.map) ||
                    waypoint.noCrossMap)
                    continue;
            } else if (waypoint.map != map || (transfer && waypoint.noCrossMap))
                continue;
            CatalogPosition point;
            if (!context.catalog.WorldPosition(waypoint.map, entry.first, point))
                continue;
            const double distance = static_cast<double>(point.x) * point.x + static_cast<double>(point.y) * point.y;
            if (distance >= nearest)
                continue;
            nearest = distance;
            x = point.x;
            y = point.y;
            z = point.z;
        }
    };
    chooseWaypoints(false);
    // Instance aliases can have no own cartography while sharing the exact
    // physical map file. Reuse a permitted shrine in that local geometry.
    if (nearest == std::numeric_limits<double>::max() && context.creationSpawn.map != map)
        chooseWaypoints(true);
    if (nearest == std::numeric_limits<double>::max() && context.creationSpawn.map != map &&
        context.defaultMapArrival) {
        SpawnPoint arrival{};
        if (context.defaultMapArrival(map, arrival)) {
            if (arrival.map != map) {
                result.lines.emplace_back("This map has no valid arrival location.");
                return false;
            }
            x = arrival.x;
            y = arrival.y;
            z = arrival.z;
            return true;
        }
    }
    if (nearest == std::numeric_limits<double>::max() && context.creationSpawn.map != map)
        result.lines.emplace_back("No known arrival for this map; using 0 0 100.");
    return true;
}
bool Worldport(const CommandContext& context, const std::string& args, CommandResult& result) {
    std::istringstream input(args);
    input.imbue(std::locale::classic());
    std::vector<std::string> fields;
    std::string field;
    while (input >> field) {
        fields.push_back(field);
        if (fields.size() > 4)
            return false;
    }
    if (fields.size() != 1 && fields.size() != 3 && fields.size() != 4)
        return false;
    unsigned map = context.character.map;
    const size_t start = fields.size() == 3 ? 0 : 1;
    if (start && (!ParseUnsigned(fields[0], 65535, map) || !map))
        return false;
    double x = 0, y = 0, z = 100;
    if (fields.size() != 1 &&
        (!Distance(fields[start], x) || !Distance(fields[start + 1], y) || !Distance(fields[start + 2], z)))
        return false;
    if (!context.catalog.Available(map)) {
        result.lines.emplace_back("That map is not a playable entry in the local catalog.");
        return true;
    }
    const bool transfer = map != context.character.map;
    if (!context.capabilities.Has(transfer ? Capability::MapTransfer : Capability::LocalTeleport)) {
        result.lines.emplace_back("This command is not available on this client.");
        return true;
    }
    if (fields.size() == 1 && !MapArrival(context, map, x, y, z, result))
        return true;
    if (!Destination(x, y, z, result))
        return false;
    if (result.teleport && transfer) {
        result.teleport = false;
        result.transferMap = map;
    }
    return true;
}
const OnlinePlayer* FindPlayer(const CommandContext& context, const std::string& name) {
    std::wstring requested;
    if (!Utf8ToWide(name, requested))
        return nullptr;
    for (const auto& player : context.players) {
        std::wstring candidate;
        if (Utf8ToWide(player.name, candidate) &&
            CompareStringOrdinal(requested.c_str(), -1, candidate.c_str(), -1, TRUE) == CSTR_EQUAL)
            return &player;
    }
    return nullptr;
}
bool Who(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (!args.empty())
        return false;
    result.lines.emplace_back("Online players: " + std::to_string(context.players.size()));
    std::vector<std::string> names;
    for (const auto& player : context.players)
        names.push_back(player.name);
    std::sort(names.begin(), names.end());
    for (const auto& name : names)
        result.lines.emplace_back("- " + name);
    return true;
}
bool Where(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (args.empty())
        return false;
    const auto* player = FindPlayer(context, args);
    if (!player) {
        result.lines.emplace_back("That player is not online. Use .who to list online players.");
        return true;
    }
    char location[160];
    std::snprintf(location, sizeof(location), " - Map %u | X: %.3f | Y: %.3f | Z: %.3f", player->map,
                  player->position.x, player->position.y, player->position.z);
    result.lines.emplace_back(player->name + location);
    return true;
}
bool Tele(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (args.empty())
        return false;
    const auto* player = FindPlayer(context, args);
    if (!player) {
        result.lines.emplace_back("That player is not online. Use .who to list online players.");
        return true;
    }
    if (!context.catalog.Available(player->map)) {
        result.lines.emplace_back("That map is not a playable entry in the local catalog.");
        return true;
    }
    const bool transfer = player->map != context.character.map;
    if (!transfer && !context.capabilities.Has(Capability::LocalTeleport)) {
        result.lines.emplace_back("This command is not available on this client.");
        return true;
    }
    if (!Destination(player->position.x, player->position.y, player->position.z, result))
        return false;
    if (result.teleport && transfer) {
        result.teleport = false;
        result.transferMap = player->map;
    }
    return true;
}
bool Inspect(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (args.empty())
        return false;
    const auto* player = FindPlayer(context, args);
    if (!player) {
        result.lines.emplace_back("That player is not online. Use .who to list online players.");
        return true;
    }
    char location[160], heading[32] = "n/a";
    double degrees;
    std::snprintf(location, sizeof(location), " | Map %u | X %.3f | Y %.3f | Z %.3f", player->map, player->position.x,
                  player->position.y, player->position.z);
    if (Degrees(player->hasFacing ? &player->facing : nullptr, degrees))
        std::snprintf(heading, sizeof(heading), "%.1f", degrees);
    std::string state = "On foot", description = std::string("Heading ") + heading;
    if (context.capabilities.Has(Capability::Glider)) {
        description += " | Glider " + std::to_string(player->glider);
        if (player->flying)
            state = "Gliding";
    }
    if (context.capabilities.Has(Capability::Mounts)) {
        description += " | Mount " + std::to_string(player->mountSkin);
        if (!player->flying && player->mountSkin)
            state = "Mounted";
    }
    result.lines.emplace_back("Inspect: " + player->name + location);
    result.lines.emplace_back(description + " | State " + state);
    return true;
}
bool Announce(const CommandContext&, const std::string& args, CommandResult& result) {
    if (args.empty())
        return false;
    result.announcement = args;
    return true;
}
bool Bring(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (args.empty())
        return false;
    const auto* player = FindPlayer(context, args);
    if (!player) {
        result.lines.emplace_back("That player is not online. Use .who to list online players.");
        return true;
    }
    if (player->name == context.character.name) {
        result.lines.emplace_back("You are already at your own location.");
        return true;
    }
    result.travelTargets = TravelTargets::Player;
    result.travelPlayer = player->name;
    result.requestedMap = context.character.map;
    result.destination = context.position;
    return true;
}
bool BringAll(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (!args.empty())
        return false;
    result.travelTargets = TravelTargets::Others;
    result.requestedMap = context.character.map;
    result.destination = context.position;
    return true;
}
bool TpAll(const CommandContext& context, const std::string& args, CommandResult& result) {
    std::istringstream input(args);
    input.imbue(std::locale::classic());
    std::vector<std::string> fields;
    std::string field;
    while (input >> field) {
        fields.push_back(field);
        if (fields.size() > 4)
            return false;
    }
    unsigned map;
    if ((fields.size() != 1 && fields.size() != 4) || !ParseUnsigned(fields[0], 65535, map) || !map)
        return false;
    double x = 0, y = 0, z = 100;
    if (fields.size() == 4 && (!Distance(fields[1], x) || !Distance(fields[2], y) || !Distance(fields[3], z)))
        return false;
    if (!context.catalog.Available(map)) {
        result.lines.emplace_back("That map is not a playable entry in the local catalog.");
        return true;
    }
    if (fields.size() == 1 && !MapArrival(context, map, x, y, z, result))
        return true;
    if (!Destination(x, y, z, result))
        return false;
    if (result.teleport) {
        result.teleport = false;
        result.travelTargets = TravelTargets::Everyone;
        result.requestedMap = map;
    }
    return true;
}
bool MarkerRequest(const std::string& args, MarkerAction action, CommandResult& result) {
    if (args.empty())
        return false;
    if (!ValidMarkerName(args)) {
        result.lines.emplace_back("Use a marker name of 1-63 UTF-8 bytes without control characters or '|'.");
        return true;
    }
    result.markerAction = action;
    result.markerName = args;
    return true;
}
bool Mark(const CommandContext&, const std::string& args, CommandResult& result) {
    return MarkerRequest(args, MarkerAction::Set, result);
}
bool Unmark(const CommandContext&, const std::string& args, CommandResult& result) {
    return MarkerRequest(args, MarkerAction::Remove, result);
}
bool Go(const CommandContext&, const std::string& args, CommandResult& result) {
    return MarkerRequest(args, MarkerAction::Go, result);
}
bool Marks(const CommandContext&, const std::string& args, CommandResult& result) {
    if (!args.empty())
        return false;
    result.markerAction = MarkerAction::List;
    return true;
}
bool Glider(const CommandContext& context, const std::string& args, CommandResult& result) {
    unsigned design;
    if (!ParseUnsigned(args, UINT16_MAX, design) || !design)
        return false;
    if (!context.catalog.gliders.count(design)) {
        result.lines.emplace_back("That glider design ID is not in the local catalog.");
        return true;
    }
    result.glider = design;
    return true;
}
bool Mount(const CommandContext& context, const std::string& args, CommandResult& result) {
    unsigned skin;
    if (!ParseUnsigned(args, UINT32_MAX, skin) || !skin)
        return false;
    const auto found = context.catalog.mounts.find(skin);
    if (context.flying) {
        result.lines.emplace_back("Land before mounting.");
        return true;
    }
    if (found == context.catalog.mounts.end() || !found->second.mount || !found->second.content) {
        result.lines.emplace_back("That mount skin ID is not in the local catalog.");
        return true;
    }
    result.changeMount = true;
    result.mountSkin = skin;
    result.lines.emplace_back("Mounted " + found->second.name + " (skin ID " + std::to_string(skin) + ").");
    return true;
}
bool Unmount(const CommandContext& context, const std::string& args, CommandResult& result) {
    if (!args.empty())
        return false;
    if (!context.mountSkin) {
        result.lines.emplace_back("You are not mounted.");
        return true;
    }
    result.changeMount = true;
    result.lines.emplace_back("Unmounted.");
    return true;
}
// Explicit access to implemented facet actions while the native modern bar is empty.
// Every request goes through the same controller as its native counterpart.
bool Facet(const CommandContext& c, const std::string& args, CommandResult& out) {
    const auto profession = c.character.profileFields.find("profession");
    if (profession == c.character.profileFields.end() || profession->second != "9") {
        out.lines.emplace_back("This command requires a Revenant.");
        return true;
    }
    if (args.empty()) {
        out.lines.emplace_back(".facet select [land|water]: select the Dragon legend in that locale.");
        out.lines.emplace_back(".facet light|darkness|strength|elements|chaos [consume]");
        out.lines.emplace_back(
            "Consumption is implemented for all five facets. Native bar availability remains pending.");
        return true;
    }
    std::istringstream input(args);
    std::string name, action, extra;
    input >> name >> action >> extra;
    if (!extra.empty())
        return false;
    if (name == "select") {
        if (!action.empty() && action != "land" && action != "water")
            return false;
        out.selectFacetLegend = true;
        out.facetWater = action == "water";
    } else {
        if (!action.empty() && action != "consume")
            return false;
        const unsigned base = name == "light"      ? 27220
                              : name == "darkness" ? 28379
                              : name == "strength" ? 26644
                              : name == "elements" ? 27014
                              : name == "chaos"    ? 27760
                                                   : 0;
        if (!base)
            return false;
        const auto rule = c.catalog.build == 167409 ? modern::FacetData<167409>::RuleFor(base)
                                                    : modern::FacetData<204489>::RuleFor(base);
        out.facetSkill = action == "consume" ? rule.alternate : base;
    }
    out.lines.emplace_back("Request submitted. Normal legend, level, energy, equipment and recharge checks apply.");
    return true;
}
bool Help(const CommandContext&, const std::string&, CommandResult&);
struct Command {
    const char* name;
    const char* alias;
    Capability needs;
    const char* usage;
    const char* help;
    bool (*run)(const CommandContext&, const std::string&, CommandResult&);
};
const Command commands[] = {
    {"glider", "", Capability::Glider, ".glider <designId>",
     "Save a glider from the local catalog; redeploy to see it.", Glider},
    {"mark", "", Capability::Markers, ".mark <name>", "Save your current position as an account marker.", Mark},
    {"marks", "", Capability::Markers, ".marks", "List your account's saved markers.", Marks},
    {"unmark", "", Capability::Markers, ".unmark <name>", "Remove a saved marker.", Unmark},
    {"go", "", Capability::MapTransfer, ".go <name>", "Travel to a saved marker.", Go},
    {"inspect", "", Capability::Inspect, ".inspect <player name>",
     "Show an online player's location and movement state.", Inspect},
    {"announce", "", Capability::Announce, ".announce <message>", "Send an attributed announcement to all players.",
     Announce},
    {"bring", "", Capability::MapTransfer, ".bring <player name>", "Request a player transfer to your location.",
     Bring},
    {"bringall", "", Capability::MapTransfer, ".bringall", "Request all other players to transfer to your location.",
     BringAll},
    {"tpall", "", Capability::MapTransfer, ".tpall <mapId> [x y z]",
     "Transfer all players, including you; map-only uses the same arrival as .worldport.", TpAll},
    {"who", "", Capability::Count, ".who", "List online players.", Who},
    {"where", "", Capability::Count, ".where <player name>", "Show an online player's current location.", Where},
    {"tele", "", Capability::MapTransfer, ".tele <player name>", "Travel to an online player's current location.",
     Tele},
    {"timeofday", "", Capability::TimeOfDay, ".timeofday [HH:MM[:SS]]", "Show or set the time for all players.",
     TimeOfDay},
    {"mount", "", Capability::Mounts, ".mount <skinId>", "Mount a skin from the local catalog.", Mount},
    {"unmount", "", Capability::Mounts, ".unmount", "Dismount the current mount.", Unmount},
    {"help", "", Capability::Count, ".help [command]", "Show available commands or help for one command.", Help},
    {"gps", "", Capability::Count, ".gps", "Show the current map ID and position.", Gps},
    {"heading", "", Capability::Count, ".heading", "Show the current facing direction.", Heading},
    {"status", "", Capability::Count, ".status", "Show current character and server state.", Status},
    {"facet", "", Capability::FacetActions,
     ".facet [select [land|water] | light|darkness|strength|elements|chaos [consume]]",
     "Use the implemented Dragon legend facets.", Facet},
    {"trait", "", Capability::CombatTraitSelection, ".trait [1453|off]",
     "Show or select the supported Warrior Might trait.", Trait},
    {"combat", "", Capability::Count, ".combat [on|off]",
     "Choose whether other combat participants are opponents; resets on world departure.", Combat},
    {"mapinfo", "", Capability::Count, ".mapinfo [mapId]", "Show local map catalog information.", MapInfo},
    {"maps", "", Capability::Count, ".maps [text or ID] [--page N]",
     "Search available maps by name or exact ID; ten per page, ordered by ID.", Maps},
    {"worldport", "", Capability::Count, ".worldport <x y z | mapId [x y z]>",
     "Travel to coordinates or a map; use a known arrival, otherwise 0 0 100 with a notice.", Worldport},
    {"up", "", Capability::LocalTeleport, ".up [heightDelta]", "Move vertically; default +100.", Up},
    {"forward", "fw", Capability::LocalTeleport, ".forward [distance]", "Move forward; default 100. Alias: .fw",
     Forward},
    {"backward", "bw", Capability::LocalTeleport, ".backward [distance]", "Move backward; default 100. Alias: .bw",
     Backward}};
bool Available(const CommandContext& context, const Command& command) {
    if (std::string(command.name) == "worldport")
        return context.capabilities.Has(Capability::LocalTeleport) || context.capabilities.Has(Capability::MapTransfer);
    return command.needs == Capability::Count || context.capabilities.Has(command.needs);
}
const Command* Find(const CommandContext& context, const std::string& name) {
    for (const auto& command : commands)
        if (Available(context, command) && (name == command.name || (*command.alias && name == command.alias)))
            return &command;
    return nullptr;
}
bool Help(const CommandContext& context, const std::string& args, CommandResult& result) {
    auto& replies = result.lines;
    if (args.empty()) {
        for (const auto& command : commands)
            if (Available(context, command))
                replies.emplace_back(std::string(command.usage) + " - " + command.help);
        return true;
    }
    std::string name = Lower(args);
    if (name[0] == '.')
        name.erase(0, 1);
    const auto* command = Find(context, name);
    if (!command)
        replies.emplace_back("Unknown command. Use .help.");
    else
        replies.emplace_back(std::string("Usage: ") + command->usage + " - " + command->help);
    return true;
}
} // namespace
bool ExecuteCommand(const CommandContext& context, const std::string& text, CommandResult& result) {
    if (text.empty() || text[0] != '.')
        return false;
    const auto end = text.find_first_of(" \t");
    const std::string name = Lower(text.substr(1, end == std::string::npos ? end : end - 1));
    const auto* command = Find(context, name);
    if (!command) {
        result = {};
        result.lines.emplace_back("Unknown command. Use .help.");
        return true;
    }
    const std::string args = end == std::string::npos ? std::string() : Trim(text.substr(end));
    CommandResult output;
    if (!command->run(context, args, output))
        output.lines.emplace_back(std::string("Usage: ") + command->usage);
    result = std::move(output);
    return true;
}
} // namespace glide
