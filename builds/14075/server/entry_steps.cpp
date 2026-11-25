#include "entry_steps.h"
#include "mission.h"
#include "world_actor.h"
#include "../../14505/server/world_actor.h"
#include "world_managers.h"
#include "inventory_wire.h"
#include "inventory_state.h"
#include "world_environment.h"
#include "glide/files.h"
#include <algorithm>

namespace glide {
namespace april14075 {
bool WorldConstructionSteps(const BootstrapContext& context, const CharacterCatalog& characters,
                            const MapGeometryCatalog& maps, std::vector<BootstrapStep>& out) {
    const auto* id = context.identity;
    const auto& character = context.entry.character;
    const auto* map = maps.Find(character.map);
    if (!id || !id->player || id->player == 255 || id->character != 256u + id->player || id->firstBag || id->itemBase ||
        context.catalog.build != 14075 || characters.Build() != 14075 || maps.Build() != 14075 ||
        !context.catalog.Available(character.map) || !map || map->id > UINT16_MAX || map->type == 1 ||
        context.catalog.maps.at(character.map).type != map->type || context.entry.previousMap > UINT16_MAX ||
        !context.sequence || context.sequence == UINT32_MAX)
        return false;
    june14505::RosterCharacter roster;
    std::string error;
    if (!characters.Resolve(character, roster, error))
        return false;
    unsigned level = character.level;
    const auto effective = character.profileFields.find("effective_level");
    if (effective != character.profileFields.end() && (!ParseUnsigned(effective->second, 80, level) || !level))
        return false;
    const SpawnPoint point{character.map, character.x, character.y, character.z};
    Buffer identity, spawn, load, actor, equipment, bag, managers, activation, begin, time, skills;
    if (!MissionIdentity(character.name, id->player, static_cast<uint16_t>(map->id), context.entry.account.homeWorld,
                         context.entry.account.worldClass, map->type == 4, identity) ||
        !MissionSpawn(point, static_cast<uint16_t>(context.entry.previousMap), spawn) ||
        !MissionLoad(point, map->bounds, 0, load) ||
        !OwnedActor(characters, character, id->player, id->character, context.sequence, actor,
                    context.entry.account.karma) ||
        !InitialInventoryEquipment(characters, character, id->player, id->character, equipment) ||
        !InitialInventoryBag(id->player, id->character, bag) || !equipment.Append(bag.View()) ||
        !OwnedManagers(id->player, id->character, roster.profession, level, context.entry.account.coins,
                       {{311, 179, 20}}, managers, equipment.View(), 1) ||
        !ActivateActor(character, id->character, context.sequence + 1, activation) || !MissionBegin(begin) ||
        !TimeOfDay(context.timeTicks, context.timeRate, context.timePaused, time))
        return false;
    // In-memory construction fixtures may omit learned data; the production
    // bootstrap requires the complete catalog before accepting a connection.
    auto learnedSkills = roster.learnedSkills;
    // Own conditional Hurl remains gated by its BuffSkill condition.
    if (roster.profession == 6)
        for (uint32_t skill : {518u, 520u, 521u, 522u, 13417u})
            if (std::find(learnedSkills.begin(), learnedSkills.end(), skill) == learnedSkills.end())
                learnedSkills.push_back(skill);
    if (!learnedSkills.empty() && !june14505::LearnWeaponSkillsForBuild(14075, learnedSkills, skills))
        return false;
    if (!june14505::InitialElementalTraitsForBuild(14075, roster.profession, roster.level, skills))
        return false;
    if (roster.profession == 6) {
        uint8_t state = 0;
        if (!june14505::SavedElementalStateForBuild(14075, character, roster.profession, roster.level, state) ||
            (state != 1 && (!skills.U16(186) || !skills.VarU32(id->character) || !skills.VarU32(state))))
            return false;
    }
    std::vector<BootstrapStep> steps;
    const auto wait = [&](const char* name, GameConfirmation confirmation) {
        BootstrapStep step;
        step.name = name;
        step.kind = BootstrapStep::Kind::WaitFor;
        step.confirmation = confirmation;
        step.timeoutMs = 120000;
        steps.push_back(std::move(step));
    };
    const auto send = [&](const char* name, Buffer&& payload) {
        BootstrapStep step;
        step.name = name;
        step.payload = std::move(payload);
        steps.push_back(std::move(step));
    };
    // The MsgConn handshake waits for C75, C74, C6a, C71 and C6d in this order.
    wait("April account identity", GameConfirmation::Hello);
    send("April map identity", std::move(identity));
    wait("April map accepted", GameConfirmation::MapContextReady);
    send("April spawn", std::move(spawn));
    wait("April spawn accepted", GameConfirmation::MapLoadAccepted);
    send("April load map", std::move(load));
    wait("April map loaded", GameConfirmation::MapLoaderReady);
    send("April owned actor", std::move(actor));
    send("April character managers and control", std::move(managers));
    wait("April world built", GameConfirmation::WorldBuilt);
    if (skills.Size())
        send("April learned weapon skills", std::move(skills));
    send("April activate actor", std::move(activation));
    send("April begin playing", std::move(begin));
    send("April time of day", std::move(time));
    // Own armor, weapons and bag are published before player control. Complete
    // skill/effect data and discovery remain required; use April content only.
    out = std::move(steps);
    return true;
}
} // namespace april14075
} // namespace glide
