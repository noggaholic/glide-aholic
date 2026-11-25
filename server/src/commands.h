#pragma once
#include "glide/catalog.h"
#include "glide/character.h"
#include "glide/profile/game_messages.h"
#include "glide/capabilities.h"
#include <vector>
#include <functional>

namespace glide {
struct OnlinePlayer {
    std::string name;
    unsigned map = 0;
    CatalogPosition position;
    Facing facing;
    bool hasFacing = false, flying = false;
    uint32_t glider = 0, mountSkin = 0;
    bool combatEnabled = false;
};
struct CommandContext {
    const Catalog& catalog;
    const Character& character;
    CatalogPosition position;
    size_t online = 0;
    const Facing* facing = nullptr;
    uint32_t timeTicks = 0;
    CapabilitySet capabilities;
    uint32_t mountSkin = 0;
    bool flying = false;
    std::vector<OnlinePlayer> players;
    SpawnPoint creationSpawn{};
    std::function<bool(unsigned, SpawnPoint&)> defaultMapArrival;
    bool combatEnabled = false;
};
enum class MarkerAction { None, Set, List, Remove, Go };
enum class TravelTargets { None, Player, Others, Everyone };
struct CommandResult {
    uint32_t glider = 0;
    MarkerAction markerAction = MarkerAction::None;
    std::string markerName;
    std::string announcement;
    TravelTargets travelTargets = TravelTargets::None;
    std::string travelPlayer;
    unsigned requestedMap = 0;
    std::vector<std::string> lines;
    bool teleport = false;
    unsigned transferMap = 0;
    bool changeTime = false;
    uint32_t timeTicks = 0;
    bool changeMount = false;
    uint32_t mountSkin = 0;
    CatalogPosition destination;
    bool changeCombat = false, combatEnabled = false;
    bool changeTrait = false;
    unsigned combatTrait = 0;
    unsigned facetSkill = 0;
    bool selectFacetLegend = false, facetWater = false;
};
// Returns false only for ordinary chat. Dot commands always produce a private reply.
bool ExecuteCommand(const CommandContext& context, const std::string& text, CommandResult& result);
} // namespace glide
