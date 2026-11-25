#include "entry_steps.h"
#include "inventory_state.h"
#include "mission.h"
#include "world_actor.h"
#include "world_managers.h"
#include "glide/files.h"
#include <algorithm>
#include <algorithm>

namespace glide {
namespace june14505 {
bool WorldConstructionSteps(const BootstrapContext& context, const CharacterCatalog& characters,
                            const MapGeometryCatalog& maps, uint8_t player, uint16_t agent, uint16_t previousMap,
                            const std::array<uint32_t, 3>& achievements, const WorldDiscovery& discovery,
                            std::vector<BootstrapStep>& out) {
    const auto& character = context.entry.character;
    const auto* map = maps.Find(character.map);
    if ((characters.Build() != 14422 && characters.Build() != 14505) || context.catalog.build != characters.Build() ||
        maps.Build() != characters.Build() || discovery.build != characters.Build() ||
        !context.catalog.Available(character.map) || !map || map->type == 1 ||
        context.catalog.maps.at(character.map).type != map->type || !context.sequence || context.sequence == UINT32_MAX)
        return false;
    RosterCharacter roster;
    std::string error;
    if (!characters.Resolve(character, roster, error))
        return false;
    auto learnedSkills = roster.learnedSkills;
    // June selectors; the automatic Display traits are a separate condition. Own conditional Hurl must be
    // learned as well as its base action. Its BuffSkill condition still controls availability; learning is not a
    // cast.
    if (roster.profession == 6)
        for (uint32_t skill : {518u, 520u, 521u, 522u, 13417u})
            if (std::find(learnedSkills.begin(), learnedSkills.end(), skill) == learnedSkills.end())
                learnedSkills.push_back(skill);
    unsigned level = character.level;
    const auto effective = character.profileFields.find("effective_level");
    if (effective != character.profileFields.end() && (!ParseUnsigned(effective->second, 80, level) || !level))
        return false;
    const SpawnPoint point{character.map, character.x, character.y, character.z};
    std::vector<BootstrapStep> steps;
    auto wait = [&](const char* name, GameConfirmation confirmation) {
        BootstrapStep step;
        step.name = name;
        step.kind = BootstrapStep::Kind::WaitFor;
        step.confirmation = confirmation;
        step.timeoutMs = 120000;
        steps.push_back(std::move(step));
    };
    auto send = [&](const char* name, Buffer&& payload) {
        BootstrapStep step;
        step.name = name;
        step.payload = std::move(payload);
        steps.push_back(std::move(step));
    };
    Buffer identity, spawn, load, actor, managers, equipment, bag, activation, begin, revealed, skills, weaponRefresh,
        time;
    if (!MissionIdentity(character.name, player, static_cast<uint16_t>(map->id), context.entry.account.homeWorld,
                         context.entry.account.worldClass, map->type == 4, identity) ||
        !MissionSpawn(point, previousMap, spawn) || !MissionLoad(point, map->bounds, 0, load) ||
        !OwnedActor(characters, character, player, agent, context.sequence, actor, context.entry.account.karma) ||
        !InitialInventoryEquipment(characters, character, player, agent, equipment) ||
        !InitialInventoryBagForBuild(characters.Build(), player, agent, bag) || !equipment.Append(bag.View()) ||
        !OwnedManagersForBuild(characters.Build(), player, agent, roster.profession, level, context.entry.account.coins,
                               achievements, managers, equipment.View(), 1) ||
        !LearnWeaponSkillsForBuild(characters.Build(), learnedSkills, skills) ||
        !RestoreEquippedInventoryWeapons(characters, character, player, agent, weaponRefresh) ||
        !DiscoveryMessage(discovery, revealed) || !ActivateActor(character, agent, context.sequence + 1, activation) ||
        !MissionBegin(begin) || !TimeOfDay(context.timeTicks, context.timeRate, context.timePaused, time) ||
        // Apply equipment callbacks after activation and the begin-playing
        // message, when the native skillbar can consume the learned definitions.
        !time.Append(weaponRefresh.View()))
        return false;
    // The skills step is sent after the owned managers and WorldBuilt gate.
    // Use actual character level: downscaling must not remove learned unlocks.
    if ((characters.Build() == 14422 || characters.Build() == 14505) &&
        !InitialElementalTraitsForBuild(characters.Build(), roster.profession, roster.level, skills))
        return false;
    if ((characters.Build() == 14422 || characters.Build() == 14505) && roster.profession == 6) {
        uint8_t state = 0;
        if (!SavedElementalStateForBuild(characters.Build(), character, roster.profession, roster.level, state) ||
            (state != 1 && (!skills.U16(216) || !skills.VarU32(agent) || !skills.VarU32(state))))
            return false;
    }
    wait("June account identity", GameConfirmation::Hello);
    send("June map identity", std::move(identity));
    wait("June map accepted", GameConfirmation::MapContextReady);
    send("June spawn", std::move(spawn));
    wait("June spawn accepted", GameConfirmation::MapLoadAccepted);
    send("June load map", std::move(load));
    wait("June map loaded", GameConfirmation::MapLoaderReady);
    send("June owned actor", std::move(actor));
    send("June character managers", std::move(managers));
    wait("June world built", GameConfirmation::WorldBuilt);
    send("June discovery", std::move(revealed));
    send("June learned weapon skills", std::move(skills));
    send("June activate actor", std::move(activation));
    send("June begin playing", std::move(begin));
    send("June time of day", std::move(time));
    out = std::move(steps);
    return true;
}
} // namespace june14505
} // namespace glide
