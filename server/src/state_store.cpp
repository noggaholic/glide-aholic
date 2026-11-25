#include "glide/profile/condition_damage.h"
#include "glide/profile/blindness.h"
#include "glide/profile/reveal_state.h"
#include "glide/profile/infuse_light.h"
#include "glide/profile/forced_engagement.h"
#include "glide/profile/jade_winds.h"
#include "glide/profile/banish_enchantment.h"
#include "glide/profile/boon_removal.h"
#include "server.h"
#include "commands.h"
#include "glide/profile/legend_state.h"
#include "glide/profile/modern_legends.h"
#include "glide/profile/modern_health.h"
#include "glide/profile/june_health.h"
#include "glide/profile/beta_judgment_data.h"
#include "glide/profile/beta_siren_data.h"
#include "glide/profile/beta_melee_combat.h"
#include "glide/profile/projectile_combat.h"
#include "glide/profile/beta_health.h"
#include "glide/profile/june_water_healing.h"
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/endurance.h"
#include "glide/profile/initiative.h"
#include "glide/profile/assassin_siphon.h"
#include <cstdio>
#include "glide/crypto.h"
#include <Windows.h>
#include <iomanip>
#include <limits>
#include <sstream>
#include <cmath>

namespace glide {
namespace {
bool ValidWorldLegends(unsigned build, const Character& active, const std::string& legends) {
    const auto profession = active.profileFields.find("profession");
    if (profession == active.profileFields.end() || profession->second != "9")
        return false;
    Character proposed = active;
    proposed.profileFields["legend_state"] = legends;
    switch (build) {
    case 49284: {
        LegendState checked;
        return checked.Load(proposed, 9);
    }
    case 167409: {
        modern::Legends<167409>::State checked;
        return checked.Load(proposed, 9);
    }
    case 204489: {
        modern::Legends<204489>::State checked;
        return checked.Load(proposed, 9);
    }
    default:
        return false;
    }
}
} // namespace
bool Server::SaveAccountValue(const std::shared_ptr<const LoginSession>& session, const char* key,
                              const std::string& value, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !accounts_) {
        error = "No active account";
        return false;
    }
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut || at->second.owners.empty()) {
        error = "Account session is no longer active";
        return false;
    }
    KvDocument document;
    if (!LoadAccountLocked(session->login, document, error))
        return false;
    if (!document.Set("", key, value)) {
        error = "Invalid account preference field";
        return false;
    }
    return SaveAccountLocked(document, error);
}
bool Server::SaveAccountConfig(const std::shared_ptr<const LoginSession>& session, ByteView bytes, std::string& error) {
    // Storage ceiling covers the known profiles; the connection applies its own wire limit.
    if (!bytes.Valid() || bytes.size > 4096) {
        error = "Invalid account configuration size";
        return false;
    }
    const char* digits = "0123456789abcdef";
    std::string hex;
    hex.reserve(bytes.size * 2);
    for (size_t i = 0; i < bytes.size; ++i) {
        hex += digits[bytes.data[i] >> 4];
        hex += digits[bytes.data[i] & 15];
    }
    return SaveAccountValue(session, "auth_config", hex, error);
}
bool Server::SaveAgreement(const std::shared_ptr<const LoginSession>& session, uint8_t version, std::string& error) {
    return SaveAccountValue(session, "agreement_version", std::to_string(version), error);
}
bool Server::SaveCompletedHint(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint8_t hint,
                               std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    error = "Cannot save tutorial for this world connection";
    if (!session || !owner || !accounts_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        at->second.worldOwner != owner || !at->second.owners.count(owner) || !at->second.worldPrepared ||
        !at->second.activeWorld)
        return false;
    KvDocument document;
    AccountState saved;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, saved, error))
        return false;
    if (saved.userGuid.Text() != at->second.activeWorld->account.userGuid.Text()) {
        error = "Saved account changed during world session";
        return false;
    }
    const uint32_t mask = uint32_t(1) << (hint % 32);
    if (!(saved.completedHints[hint / 32] & mask)) {
        saved.completedHints[hint / 32] |= mask;
        static const char digits[] = "0123456789abcdef";
        std::string hex;
        hex.reserve(64);
        for (uint32_t word : saved.completedHints)
            for (unsigned byte = 0; byte < 4; ++byte) {
                const auto value = static_cast<uint8_t>(word >> (byte * 8));
                hex += digits[value >> 4];
                hex += digits[value & 15];
            }
        if (!document.Set("", "completed_hints", hex) || !SaveAccountLocked(document, error))
            return false;
    }
    auto& account = at->second.activeWorld->account;
    account.completedHints = saved.completedHints;
    account.document = std::move(document);
    error.clear();
    return true;
}
namespace {
std::string Position(float value) {
    std::ostringstream text;
    text.imbue(std::locale::classic());
    text << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return text.str();
}
bool Store(KvDocument& document, const Character& character) {
    const std::string section = "character " + std::to_string(character.slot);
    for (const auto& field : character.profileFields)
        if (!document.Set(section, field.first, field.second))
            return false;
    if (!character.native.empty()) {
        const char* hex = "0123456789abcdef";
        std::string bytes;
        for (uint8_t byte : character.native) {
            bytes += hex[byte >> 4];
            bytes += hex[byte & 15];
        }
        if (!document.Set(section, "native", bytes))
            return false;
    }
    return document.Set(section, "name", character.name) && document.Set(section, "guid", character.guid.Text(false)) &&
           document.Set(section, "level", std::to_string(character.level)) &&
           document.Set(section, "map", std::to_string(character.map)) &&
           document.Set(section, "x", Position(character.x)) && document.Set(section, "y", Position(character.y)) &&
           document.Set(section, "z", Position(character.z)) &&
           document.Set(section, "glider", std::to_string(character.glider)) &&
           document.Set(section, "mount", std::to_string(character.mount));
}
} // namespace
bool Server::SetGliderLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint32_t design,
                             std::string& error) {
    error = "Cannot change glider for this world connection";
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.worldOwner != owner ||
        !at->second.worldReady || !at->second.activeWorld || at->second.loggedOut || !catalog_.gliders.count(design))
        return false;
    auto& entry = *at->second.activeWorld;
    KvDocument document;
    AccountState saved;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, saved, error))
        return false;
    const auto& live = entry.character;
    const Character* selected = nullptr;
    for (const auto& character : saved.characters)
        if (character.slot == live.slot)
            selected = &character;
    if (!selected || saved.selected != live.slot || selected->guid.Text() != live.guid.Text() ||
        selected->map != at->second.savedMap) {
        error = "Saved character changed during world session";
        return false;
    }
    if (!document.Set("character " + std::to_string(live.slot), "glider", std::to_string(design)) ||
        !SaveAccountLocked(document, error))
        return false;
    entry.character.glider = design;
    entry.character.profileFields["glider"] = std::to_string(design);
    for (auto& character : entry.account.characters)
        if (character.slot == live.slot) {
            character.glider = design;
            character.profileFields["glider"] = std::to_string(design);
        }
    entry.account.document = std::move(document);
    error.clear();
    return true;
}
void Server::MarkerRepliesLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                 const CatalogPosition& position, CommandResult& command) {
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.worldOwner != owner ||
        !at->second.worldReady || !at->second.activeWorld || at->second.loggedOut)
        return;
    KvDocument document;
    AccountState saved;
    std::string error;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, saved, error)) {
        QueueLogLocked("markers", error);
        command.lines.emplace_back("Saved markers could not be read.");
        return;
    }
    auto& markers = saved.markers;
    auto found = markers.begin();
    for (; found != markers.end(); ++found)
        if (MarkerNameEqual(found->name, command.markerName))
            break;
    if (command.markerAction == MarkerAction::List) {
        if (markers.empty())
            command.lines.emplace_back("No saved markers.");
        else
            command.lines.emplace_back("Saved markers: " + std::to_string(markers.size()));
        for (const auto& marker : markers) {
            char location[160];
            std::snprintf(location, sizeof(location), " | Map %u | %.3f %.3f %.3f", marker.map, marker.x, marker.y,
                          marker.z);
            command.lines.emplace_back("- " + marker.name + location);
        }
        return;
    }
    if (command.markerAction != MarkerAction::Set && found == markers.end()) {
        command.lines.emplace_back("Marker not found. Use .marks to list saved markers.");
        return;
    }
    if (command.markerAction == MarkerAction::Go) {
        if (!catalog_.Available(found->map)) {
            command.lines.emplace_back("That map is not a playable entry in the local catalog.");
            return;
        }
        if (found->map == at->second.activeWorld->character.map) {
            if (!profile_->Capabilities().Has(Capability::LocalTeleport)) {
                command.lines.emplace_back("This command is not available on this client.");
                return;
            }
            command.teleport = true;
        } else
            command.transferMap = found->map;
        command.destination = {found->x, found->y, found->z};
        return;
    }
    if (command.markerAction == MarkerAction::Remove)
        markers.erase(found);
    else {
        const unsigned map = at->second.activeWorld->character.map;
        if (!catalog_.Available(map) || !std::isfinite(position.x * 32) || !std::isfinite(position.y * 32) ||
            !std::isfinite(position.z * 32)) {
            command.lines.emplace_back("This position cannot be saved as a marker.");
            return;
        }
        Marker marker;
        marker.name = command.markerName;
        marker.map = map;
        marker.x = position.x;
        marker.y = position.y;
        marker.z = position.z;
        if (found == markers.end())
            markers.push_back(std::move(marker));
        else
            *found = std::move(marker);
    }
    std::vector<std::string> values;
    for (const auto& marker : markers)
        values.push_back(marker.name + "|" + std::to_string(marker.map) + "|" + Position(marker.x) + "|" +
                         Position(marker.y) + "|" + Position(marker.z));
    if (!document.SetValues("", "marker", values) || !SaveAccountLocked(document, error)) {
        QueueLogLocked("markers", error);
        command.lines.emplace_back("Markers could not be saved. The previous markers are unchanged.");
        return;
    }
    command.lines.emplace_back(
        std::string(command.markerAction == MarkerAction::Remove ? "Marker removed: " : "Marker saved: ") +
        command.markerName);
}
CreateResult Server::CreateCharacter(const std::shared_ptr<const LoginSession>& session, Character character,
                                     Character& out, std::string& error, uint64_t creationOwner) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    error = "Cannot create character for this session";
    if (!session || !accounts_ || !profile_)
        return CreateResult::Failed;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut || at->second.owners.empty())
        return CreateResult::Failed;
    if (at->second.worldOwner)
        return CreateResult::Failed;
    if ((creationOwner || at->second.createOwner) &&
        (!creationOwner || creationOwner != at->second.createOwner || at->second.createSaved))
        return CreateResult::Failed;
    if (!Creation().ValidateSubmit(character)) {
        error = "Invalid character submission";
        return CreateResult::Failed;
    }
    if (!catalog_.Available(character.map)) {
        error = "Character destination map is unavailable";
        return CreateResult::Failed;
    }
    KvDocument document;
    AccountState state;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, state, error))
        return CreateResult::Failed;
    bool used[5] = {};
    for (const auto& existing : state.characters)
        used[existing.slot] = true;
    unsigned slot = 0;
    while (slot < 5 && used[slot])
        ++slot;
    if (slot == 5) {
        error = "All five character slots are occupied";
        return CreateResult::Full;
    }
    std::wstring name;
    if (!Utf8ToWide(character.name, name))
        return CreateResult::Failed;
    if (!RandomBytes(character.guid.bytes, sizeof(character.guid.bytes)))
        return CreateResult::Failed;
    CharacterReservation reservation(*this, name, character.guid);
    if (reservation.Result() != CreateResult::Saved) {
        error = reservation.Result() == CreateResult::DuplicateName ? "Character name is already in use"
                                                                    : "Character identity collision";
        return reservation.Result();
    }
    std::vector<std::string> logins;
    if (!ListAccountsLocked(logins, error))
        return CreateResult::Failed;
    for (const auto& login : logins) {
        KvDocument stored;
        AccountState other;
        if (!LoadAccountLocked(login, stored, error) || !DecodeAccountState(stored, other, error))
            return CreateResult::Failed;
        for (const auto& existing : other.characters) {
            std::wstring existingName;
            if (!Utf8ToWide(existing.name, existingName))
                return CreateResult::Failed;
            if (CompareStringOrdinal(name.c_str(), -1, existingName.c_str(), -1, TRUE) == CSTR_EQUAL) {
                error = "Character name is already in use";
                return CreateResult::DuplicateName;
            }
            if (character.guid.Text(false) == existing.guid.Text(false)) {
                error = "Character identity collision";
                return CreateResult::Failed;
            }
        }
    }
    character.slot = slot;
    const CharacterCodec& codec =
        characterCodec_ ? *characterCodec_ : static_cast<const CharacterCodec&>(profile_->Auth());
    if (!codec.InitializeInventory(character, error))
        return CreateResult::Failed;
    if (!Store(document, character) || !document.Set("", "selected", std::to_string(slot)) ||
        !DecodeAccountState(document, state, error))
        return CreateResult::Failed;
    Buffer reply;
    if (!Creation().Created(state.userGuid, character, reply)) {
        error = "Cannot encode created character";
        return CreateResult::Failed;
    }
    // Name ownership and slot allocation become durable together; a failed rename reserves neither.
    if (!SaveAccountLocked(document, error))
        return CreateResult::Failed;
    if (creationOwner) {
        at->second.createSaved = true;
        at->second.createdCharacter = character.guid;
    }
    at->second.worldEntry.association = 0;
    out = std::move(character);
    error.clear();
    return CreateResult::Saved;
}
DeleteResult Server::DeleteCharacter(const std::shared_ptr<const LoginSession>& session, const Guid& character,
                                     AccountState& out, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    error = "Cannot delete character for this session";
    if (!session || !accounts_)
        return DeleteResult::Failed;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut || at->second.owners.empty())
        return DeleteResult::Failed;
    auto& live = at->second;
    const auto identity = character.Text();
    if ((live.activeWorld && live.activeWorld->character.guid.Text() == identity) ||
        (live.worldEntry.association && live.worldExpires > clock_() &&
         live.worldEntry.character.guid.Text() == identity) ||
        (live.createOwner && live.createSaved && live.createdCharacter.Text() == identity)) {
        error = "Character is playing or entering a map";
        return DeleteResult::Busy;
    }
    KvDocument document;
    AccountState state;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, state, error))
        return DeleteResult::Failed;
    const Character* selected = nullptr;
    for (const auto& value : state.characters)
        if (value.guid.Text() == identity)
            selected = &value;
    if (!selected) {
        error = "Character does not belong to this account";
        return DeleteResult::NotFound;
    }
    const unsigned slot = selected->slot;
    if (!document.RemoveSection("character " + std::to_string(slot)))
        return DeleteResult::Failed;
    if (state.selected == slot) {
        unsigned replacement = 0;
        for (const auto& value : state.characters)
            if (value.slot != slot) {
                replacement = value.slot;
                break;
            }
        if (!document.Set("", "selected", std::to_string(replacement)))
            return DeleteResult::Failed;
    }
    if (!DecodeAccountState(document, state, error) || !SaveAccountLocked(document, error))
        return DeleteResult::Failed;
    // The durable roster is also the name registry. Publish only after the atomic save.
    out = std::move(state);
    error.clear();
    return DeleteResult::Saved;
}
bool Server::SaveWorldPosition(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                               const CatalogPosition& position, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    return SaveWorldPositionLocked(session, owner, position, error);
}
bool Server::SaveWorldPositionLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                     const CatalogPosition& position, std::string& error, bool entering) {
    error = "Cannot save world position for this connection";
    if (!session || !owner || !accounts_ || !std::isfinite(position.x * 32) || !std::isfinite(position.y * 32) ||
        !std::isfinite(position.z * 32))
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.worldOwner != owner ||
        !at->second.activeWorld ||
        (!at->second.worldReady && !(entering && at->second.worldPrepared && at->second.activeWorld->transfer)))
        return false;
    auto& active = at->second.activeWorld->character;
    if (!catalog_.Available(active.map))
        return false;
    combat::StunState control;
    if (!control.Load(profile_->Info().build, combat::StunState::Field(active)))
        return false;
    const CatalogPosition actual =
        control.Active(wallClock_()) ? CatalogPosition{active.x, active.y, active.z} : position;
    KvDocument document;
    AccountState state;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, state, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : state.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || state.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != at->second.savedMap) {
        error = "Saved character changed during world session";
        return false;
    }
    const std::string section = "character " + std::to_string(active.slot);
    if (!document.Set(section, "map", std::to_string(active.map)) || !document.Set(section, "x", Position(actual.x)) ||
        !document.Set(section, "y", Position(actual.y)) || !document.Set(section, "z", Position(actual.z)) ||
        !SaveAccountLocked(document, error))
        return false;
    at->second.savedMap = active.map;
    active.x = actual.x;
    active.y = actual.y;
    active.z = actual.z;
    // Keep the retained text fields consistent with the durable coordinates.
    active.profileFields["map"] = std::to_string(active.map);
    active.profileFields["x"] = Position(actual.x);
    active.profileFields["y"] = Position(actual.y);
    active.profileFields["z"] = Position(actual.z);
    error.clear();
    return true;
}
bool Server::SaveWorldInventoryLocked(SessionState& activeState, const std::string& locations,
                                      const std::string& bindings, std::string& error) {
    error = "Cannot save inventory for this connection";
    if (!accounts_ || !characterCodec_ || !profile_ || !profile_->SupportsInventoryPersistence() ||
        !activeState.session || activeState.loggedOut || !activeState.worldOwner || !activeState.worldReady ||
        !activeState.activeWorld || locations.size() > 512 || bindings.size() > 255)
        return false;
    auto& active = activeState.activeWorld->character;
    KvDocument document;
    AccountState account;
    if (!LoadAccountLocked(activeState.session->login, document, error) ||
        !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != activeState.savedMap)
        return false;
    const auto current = [](const Character& character, const char* field) {
        const auto at = character.profileFields.find(field);
        return at == character.profileFields.end() ? std::string() : at->second;
    };
    if (current(*saved, "inventory_locations") != current(active, "inventory_locations") ||
        current(*saved, "inventory_bound") != current(active, "inventory_bound")) {
        error = "Saved inventory changed during world session";
        return false;
    }
    Character candidate = active;
    candidate.profileFields["inventory_locations"] = locations;
    candidate.profileFields["inventory_bound"] = bindings;
    Buffer summary;
    if (!characterCodec_->Summary(candidate, summary)) {
        error = "Invalid saved inventory locations";
        return false;
    }
    const std::string section = "character " + std::to_string(active.slot);
    if (!document.Set(section, "inventory_locations", locations) ||
        !document.Set(section, "inventory_bound", bindings) || !SaveAccountLocked(document, error))
        return false;
    active.profileFields["inventory_locations"] = locations;
    active.profileFields["inventory_bound"] = bindings;
    error.clear();
    return true;
}
bool Server::SaveWorldSkillRechargesLocked(SessionState& activeState, const std::string& deadlines, std::string& error,
                                           bool releaseState) {
    error = "Cannot save skill recharges for this connection";
    if (!accounts_ || !characterCodec_ || !profile_ || !profile_->SupportsSkillRechargePersistence() ||
        !activeState.session || activeState.loggedOut || !activeState.worldOwner || !activeState.worldReady ||
        !activeState.activeWorld || deadlines.size() > 1024)
        return false;
    auto& active = activeState.activeWorld->character;
    KvDocument document;
    AccountState account;
    if (!(releaseState ? LoadAccountLocked(activeState.session->login, document, error)
                       : accounts_->Load(activeState.session->login, document, error)) ||
        !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != activeState.savedMap)
        return false;
    const auto current = [](const Character& character) {
        const auto at = character.profileFields.find("skill_recharges");
        return at == character.profileFields.end() ? std::string() : at->second;
    };
    if (current(*saved) != current(active)) {
        error = "Saved skill recharges changed during world session";
        return false;
    }
    if (!document.Set("character " + std::to_string(active.slot), "skill_recharges", deadlines) ||
        !(releaseState ? SaveAccountLocked(document, error) : accounts_->Save(document, error)))
        return false;
    active.profileFields["skill_recharges"] = deadlines;
    error.clear();
    return true;
}
bool Server::SaveWorldLegendStateLocked(SessionState& activeState, const std::string& legends, std::string& error,
                                        bool releaseState) {
    error = "Cannot save legends for this connection";
    if (!accounts_ || !characterCodec_ || !profile_ || !profile_->SupportsLegendStatePersistence() ||
        !activeState.session || activeState.loggedOut || !activeState.worldOwner || !activeState.worldReady ||
        !activeState.activeWorld || legends.size() > 32)
        return false;
    auto& active = activeState.activeWorld->character;
    if (!ValidWorldLegends(profile_->Info().build, active, legends))
        return false;
    KvDocument document;
    AccountState account;
    if (!(releaseState ? LoadAccountLocked(activeState.session->login, document, error)
                       : accounts_->Load(activeState.session->login, document, error)) ||
        !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != activeState.savedMap)
        return false;
    const auto current = [](const Character& c) {
        const auto at = c.profileFields.find("legend_state");
        return at == c.profileFields.end() ? std::string() : at->second;
    };
    if (current(*saved) != current(active)) {
        error = "Saved legends changed during world session";
        return false;
    }
    if (!document.Set("character " + std::to_string(active.slot), "legend_state", legends) ||
        !(releaseState ? SaveAccountLocked(document, error) : accounts_->Save(document, error)))
        return false;
    active.profileFields["legend_state"] = legends;
    error.clear();
    return true;
}
bool Server::SaveWorldCombatTraitLocked(SessionState& state, unsigned selection, std::string& error) {
    error = "Cannot save combat trait selection";
    if (!accounts_ || !profile_ || !state.session || state.loggedOut || !state.worldOwner || !state.worldReady ||
        !state.activeWorld || (selection && selection != combat::kPinnacleOfStrength))
        return false;
    auto& active = state.activeWorld->character;
    unsigned current = 0;
    if (!combat::ReadCombatTrait(profile_->Info().build, active, current) ||
        (selection && !combat::CanSelectCombatTrait(profile_->Info().build, active)))
        return false;
    KvDocument document;
    AccountState account;
    if (!LoadAccountLocked(state.session->login, document, error) || !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& c : account.characters)
        if (c.slot == active.slot)
            saved = &c;
    unsigned stored = 0;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != state.savedMap || !combat::ReadCombatTrait(profile_->Info().build, *saved, stored) ||
        stored != current) {
        error = "Saved combat trait changed during world session";
        return false;
    }
    if (current == selection) {
        error.clear();
        return true;
    }
    const auto text = selection ? std::to_string(selection) : std::string();
    if (!document.Set("character " + std::to_string(active.slot), "combat_trait", text) ||
        !SaveAccountLocked(document, error))
        return false;
    active.profileFields["combat_trait"] = text;
    error.clear();
    return true;
}
bool Server::SaveWorldProfessionStateLocked(SessionState& activeState, uint8_t selection, std::string& error,
                                            bool releaseState) {
    error = "Cannot save profession state for this connection";
    if (!accounts_ || !characterCodec_ || !profile_ || !profile_->SupportsProfessionStatePersistence() ||
        !activeState.session || activeState.loggedOut || !activeState.worldOwner || !activeState.worldReady ||
        !activeState.activeWorld || selection < 1 || selection > 4)
        return false;
    auto& active = activeState.activeWorld->character;
    KvDocument document;
    AccountState account;
    if (profile_->Info().build == 14075 || profile_->Info().build == 14422 || profile_->Info().build == 14505) {
        const auto profession = active.profileFields.find("profession");
        Character candidate = active;
        candidate.profileFields["profession_state"] = std::to_string(selection);
        Buffer summary;
        if (profession == active.profileFields.end() || profession->second != "6" ||
            !characterCodec_->Summary(candidate, summary)) {
            error = "Invalid beta elemental selection";
            return false;
        }
    }
    if (!(releaseState ? LoadAccountLocked(activeState.session->login, document, error)
                       : accounts_->Load(activeState.session->login, document, error)) ||
        !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != activeState.savedMap)
        return false;
    const auto current = [](const Character& character) {
        const auto at = character.profileFields.find("profession_state");
        return at == character.profileFields.end() ? std::string() : at->second;
    };
    if (current(*saved) != current(active)) {
        error = "Saved profession state changed during world session";
        return false;
    }
    const auto value = std::to_string(selection);
    if (!document.Set("character " + std::to_string(active.slot), "profession_state", value) ||
        !(releaseState ? SaveAccountLocked(document, error) : accounts_->Save(document, error)))
        return false;
    active.profileFields["profession_state"] = value;
    error.clear();
    return true;
}
WorldActionResult Server::TickConditionDamageLocked(SessionState& state, uint64_t utc) {
    if (!profile_ || !combat::ConditionDamage::Build(profile_->Info().build) ||
        !profile_->SupportsHealthStatePersistence() || !profile_->SupportsConditionStatePersistence())
        return WorldActionResult::Ignored;
    const auto build = profile_->Info().build;
    const auto& character = state.activeWorld->character;
    const auto field = [&](const char* key) {
        const auto at = character.profileFields.find(key);
        return at == character.profileFields.end() ? std::string() : at->second;
    };
    const auto beforeClock = field("condition_damage_clock"), beforeHealth = field("health_state"),
               conditions = field("condition_effects");
    const float maximum = beta::CombatMaximumHealth(build);
    float health = 0;
    combat::ConditionDamage::Prepared next;
    bool convert = false;
    if (!combat::InfuseLight::Active(build, field("skill_effects"), utc, convert) ||
        !modern::ReadHealthState(beforeHealth, health, maximum) ||
        !combat::ConditionDamage::Prepare(build, beforeClock, conditions, utc, !state.conditionDamageReady, health,
                                          maximum, next, convert))
        return WorldActionResult::Failed;
    if (!next.changed && next.hits.empty()) {
        state.conditionDamageReady = true;
        state.conditionDamagePending = false;
        return WorldActionResult::Ignored;
    }
    Buffer owned, peers;
    const auto actor = state.peerIdentity->character;
    for (const auto& hit : next.hits)
        if (!beta::CombatHealthUpdate(build, beta::CombatLocalActor(build, actor), hit.definition, hit.before,
                                      hit.after, owned) ||
            !beta::CombatHealthUpdate(build, actor, hit.definition, hit.before, hit.after, peers))
            return WorldActionResult::Failed;
    std::string savedHealth = beforeHealth, error;
    if (!next.hits.empty() && !modern::WriteHealthState(next.health, savedHealth, maximum))
        return WorldActionResult::Failed;
    if (!SaveWorldSkillStateLocked(state, field("skill_recharges"), field("skill_effects"), error, true, &savedHealth,
                                   0, &conditions, nullptr, nullptr, nullptr, nullptr, false, &next.clock)) {
        state.conditionDamagePending = true;
        QueueLogLocked("condition-damage", error);
        return WorldActionResult::Handled;
    }
    state.conditionDamageReady = true;
    state.conditionDamagePending = false;
    if (owned.Size())
        QueuePeerLocked(state, std::make_shared<Buffer>(std::move(owned)));
    if (peers.Size())
        BroadcastPeerLocked(state, std::move(peers));
    return state.outgoingOverflow ? WorldActionResult::Failed : WorldActionResult::Ignored;
}
WorldConditionCommit Server::SaveAlliedHealthLocked(SessionState& state, const WorldActionContext& context,
                                                    const std::string& proposedHealth,
                                                    const std::vector<WorldHealthChange>& requestedTargets,
                                                    std::string& error, unsigned skill, bool hostile,
                                                    bool hostileArea) {
    const auto failed = WorldConditionCommit::NotApplied;
    std::string health = proposedHealth;
    auto targets = requestedTargets;
    error = "Allied health could not be prepared";
    const bool judgment = skill == beta::Judgment4522::Skill;
    const bool siren = profile_ && beta::SirenHealingBuild(profile_->Info().build) && skill == beta::Siren4606::Skill;
    const bool combat = hostile && profile_ &&
                        (beta::MeleeCombatSkill(profile_->Info().build, skill) ||
                         glide::combat::ProjectileSkill(profile_->Info().build, skill));
    const bool counter = combat && hostileArea && skill == 965 && beta::BetaHealthBuild(profile_->Info().build);
    const bool banish = combat && skill == glide::combat::BanishEnchantment::Skill;
    const bool jade = combat && skill == glide::combat::JadeWinds::Skill;
    const bool elemental = combat && skill == glide::combat::ElementalBlast::Skill;
    const bool burst = combat && skill == glide::combat::BurstStrengthImpact::Skill;
    const bool leap = combat && skill == glide::combat::DemonLeapImpact::Skill;
    if (hostileArea && !counter && !banish && !jade && !elemental && !burst && !leap)
        return failed;
    if (banish)
        for (const auto& hit : targets)
            if (hit.blocked)
                return failed;
    const bool area = combat && beta::MeleeAreaHealing(profile_->Info().build, skill);
    const unsigned limit = (counter || banish || jade || elemental || burst || leap)
                               ? unsigned(beta::kCounterTargetLimit)
                           : area     ? unsigned(beta::kMeleeHealingLimit) + 1
                           : combat   ? 2u
                           : siren    ? 1u
                           : judgment ? unsigned(beta::Judgment4522::BatchLimit)
                                      : 5u;
    if (!accounts_ || !profile_ || (!combat && !beta::BetaHealthBuild(profile_->Info().build)) ||
        !profile_->SupportsHealthStatePersistence() || (hostile && !combat) ||
        (!combat && skill != 1155 && !judgment && !siren) || targets.size() > limit ||
        (combat && (targets.empty() ||
                    (!area && !counter && !banish && !jade && !elemental && !burst && !leap && targets.size() != 1))))
        return failed;
    const auto field = [](const Character& value) {
        const auto at = value.profileFields.find("health_state");
        return at == value.profileFields.end() ? std::string() : at->second;
    };
    const float maximum = beta::CombatMaximumHealth(profile_->Info().build);
    const auto ready = [](const SessionState& value) {
        return value.session && !value.loggedOut && value.worldOwner && value.worldReady && value.activeWorld &&
               value.peerIdentity && !value.outgoingOverflow;
    };
    if (!ready(state))
        return failed;
    const auto conditionsAt = state.activeWorld->character.profileFields.find("condition_effects");
    const std::string beforeConditions =
        conditionsAt == state.activeWorld->character.profileFields.end() ? std::string() : conditionsAt->second;
    glide::combat::Blindness::Prepared blind;
    if (combat && !glide::combat::Blindness::Prepare(profile_->Info().build, beforeConditions, context.wallNow, blind))
        return failed;
    const bool blinded = combat && blind.instance != 0;
    if (combat && beforeConditions != context.conditionState)
        return failed;
    for (size_t i = 0; i < targets.size(); ++i) {
        const bool damage = combat && (counter || banish || jade || elemental || burst || leap || i == 0);
        if (targets[i].missed != (damage && blinded) ||
            (targets[i].missed && (targets[i].blocked || targets[i].evaded)))
            return failed;
    }
    if (combat && !counter && !blinded && targets.size() == 1 && (targets.front().blocked || targets.front().evaded)) {
        const auto& hit = targets.front();
        float ownHealth = 0, targetHealth = 0;
        if (health != context.healthState || field(state.activeWorld->character) != context.healthState ||
            hit.before != hit.after || (hit.blocked && hit.evaded) || !state.combatEnabled ||
            !modern::ReadHealthState(health, ownHealth, maximum) || ownHealth <= 0 ||
            !modern::ReadHealthState(hit.before, targetHealth, maximum) || targetHealth <= 0)
            return failed;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (&peer == &state || !ready(peer) || !peer.peerAnnounced || !peer.combatEnabled || !peer.worldActions ||
                peer.peerIdentity->character != hit.agent ||
                peer.activeWorld->character.map != state.activeWorld->character.map ||
                peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner) != hit.identity ||
                field(peer.activeWorld->character) != hit.before)
                continue;
            const bool evading = peer.worldActions->EvadesIncoming(context.now);
            if (hit.evaded) {
                if (evading)
                    return WorldConditionCommit::Applied;
                continue;
            }
            if (evading || !peer.worldActions->BlocksIncoming(context.now))
                continue;
            const auto identity = state.activeWorld->character.guid.Text() + ":" + std::to_string(state.worldOwner);
            return peer.worldActions->BlockIncoming(state.peerIdentity->character, identity, context.now)
                       ? WorldConditionCommit::Applied
                       : failed;
        }
        return failed;
    }
    unsigned level = state.activeWorld->character.level;
    // Modern actions and the published actor use saved.level. The legacy
    // effective_level field belongs to beta level scaling/starting recipes.
    if (judgment || (combat && beta::BetaHealthBuild(profile_->Info().build))) {
        const auto effective = state.activeWorld->character.profileFields.find("effective_level");
        if (effective != state.activeWorld->character.profileFields.end()) {
            level = 0;
            if (effective->second.empty())
                return failed;
            for (char digit : effective->second) {
                if (digit < '0' || digit > '9' || level > 80)
                    return failed;
                level = level * 10 + digit - '0';
            }
        }
    }
    float amount = combat     ? beta::MeleeCombatDamage()
                   : siren    ? beta::SirenHealingAmount()
                   : judgment ? beta::Judgment4522::Amount(level)
                              : float(beta::WaterGroundHealing::Amount);
    const auto ownEffectsAt = state.activeWorld->character.profileFields.find("skill_effects");
    const std::string ownEffects =
        ownEffectsAt == state.activeWorld->character.profileFields.end() ? std::string() : ownEffectsAt->second;
    unsigned selectedCombatTrait = 0;
    if (combat &&
        !glide::combat::ReadCombatTrait(profile_->Info().build, state.activeWorld->character, selectedCombatTrait))
        return failed;
    if (combat && !glide::combat::AttackMultiplier(profile_->Info().build, ownEffects, context.wallNow, level, amount,
                                                   selectedCombatTrait))
        return failed;
    if (!amount || ((judgment || siren) && health != context.healthState))
        return failed;
    float ownerHealth = 0;
    if (combat) {
        std::string expected;
        if (!level || level > 80 || !modern::ReadHealthState(context.healthState, ownerHealth, maximum) ||
            ownerHealth <= 0 ||
            !modern::WriteHealthState(
                (std::min)(maximum,
                           ownerHealth +
                               (blinded ? 0.0f : beta::MeleeCombatHealing(profile_->Info().build, skill, level))),
                expected, maximum) ||
            (health != expected && !(ownerHealth == maximum && health == context.healthState)))
            return failed;
    }
    const float healing =
        combat ? (blinded ? 0.0f : beta::MeleeCombatHealing(profile_->Info().build, skill, level)) : amount;
    const auto valid = [maximum, amount, healing](const std::string& before, const std::string& after, bool changed,
                                                  bool damage = false) {
        float a = 0, b = 0;
        return modern::ReadHealthState(before, a, maximum) && modern::ReadHealthState(after, b, maximum) &&
               (changed ? (damage ? (a > 0 && b >= 0 && b <= maximum)
                                  : (a > 0 && b > a && b <= (std::min)(maximum, a + healing)))
                        : (b == a || (a > 0 && b > a && b <= (std::min)(maximum, a + healing))));
    };
    if (!ready(state) || (!combat && !valid(context.healthState, health, false)) ||
        field(state.activeWorld->character) != context.healthState ||
        targets.size() +
                (combat ? (ownerHealth < maximum && healing > 0 ? 1 : 0) : (health != context.healthState ? 1 : 0)) >
            limit)
        return failed;
    glide::combat::AssassinSiphon siphon;
    bool siphonApplied = false;
    if (combat && !siphon.Load(profile_->Info().build, state.activeWorld->character))
        return failed;
    const bool siphonReady = combat && siphon.Ready(context.wallNow);
    Buffer siphonOwned, siphonPeers;
    std::vector<std::unique_ptr<PreparedHostileEffects>> hostileEffects(targets.size());
    std::vector<std::unique_ptr<PreparedWorldControl>> controls(targets.size());
    std::vector<std::string> beforeControls(targets.size()), afterControls(targets.size());
    std::vector<std::string> beforeTaunts(targets.size()), afterTaunts(targets.size());
    const auto extra = [](const Character& c, const char* key) {
        const auto at = c.profileFields.find(key);
        return at == c.profileFields.end() ? std::string() : at->second;
    };
    std::vector<SessionState*> participants{&state};
    std::set<uint16_t> agents;
    std::vector<std::shared_ptr<std::mutex>> mutexes;
    std::vector<std::unique_lock<std::mutex>> locks;
    std::vector<Buffer> updates, ownedUpdates, sourceUpdates;
    for (size_t i = 0; i < targets.size(); ++i) {
        auto& change = targets[i];
        const bool damage = combat && (counter || banish || jade || elemental || burst || leap || i == 0);
        const bool blocked = damage && change.blocked, evaded = damage && change.evaded,
                   immune = blocked || evaded || change.missed;
        float blockedHealth = 0;
        if (((change.blocked || change.evaded) && !damage) || (blocked && evaded))
            return failed;
        if (!change.agent || !agents.insert(change.agent).second ||
            (immune ? !(change.before == change.after &&
                        modern::ReadHealthState(change.before, blockedHealth, maximum) && blockedHealth > 0)
                    : !valid(change.before, change.after, true, damage)))
            return failed;
        SessionState* found = nullptr;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (&peer != &state && ready(peer) && peer.peerAnnounced && peer.peerIdentity->character == change.agent &&
                peer.activeWorld->character.map == state.activeWorld->character.map &&
                (damage ? (state.combatEnabled && peer.combatEnabled) : !(state.combatEnabled && peer.combatEnabled)) &&
                peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner) == change.identity)
                found = &peer;
        }
        if (!found || field(found->activeWorld->character) != change.before)
            return failed;
        if (counter) {
            double distance = 0;
            const auto& position = found->activeWorld->character;
            if (!beta::CounterAreaContains(context, {position.x, position.y, position.z}, distance))
                return failed;
        }
        if (damage && !change.missed) {
            const bool guards = !banish && found->worldActions && found->worldActions->BlocksIncoming(context.now);
            const bool evades = found->worldActions && found->worldActions->EvadesIncoming(context.now);
            if (evades != evaded || (!evaded && guards != blocked))
                return failed;
        }
        if (damage && !immune) {
            error = "Combat impact damage no longer matches saved effects";
            const auto effect = found->activeWorld->character.profileFields.find("skill_effects");
            const std::string effects =
                effect == found->activeWorld->character.profileFields.end() ? std::string() : effect->second;
            float expectedDamage = 0, beforeHealth = 0;
            std::string expected;
            if (!glide::combat::HitDamage(profile_->Info().build, ownEffects, level, effects,
                                          beta::CombatLevel(profile_->Info().build, found->activeWorld->character),
                                          context.wallNow, expectedDamage, selectedCombatTrait) ||
                !modern::ReadHealthState(change.before, beforeHealth, maximum) ||
                !glide::combat::InfuseLight::HitHealth(profile_->Info().build, effects, context.wallNow, beforeHealth,
                                                       expectedDamage, maximum, expected) ||
                expected != change.after)
                return failed;
        }
        if (area && !damage) {
            const auto& position = found->activeWorld->character;
            const double distance = std::hypot(std::hypot((double(position.x) - context.position.x) * 32,
                                                          (double(position.y) - context.position.y) * 32),
                                               (double(position.z) - context.position.z) * 32);
            if (!std::isfinite(distance) || distance > beta::MeleeHealingRadius(profile_->Info().build))
                return failed;
        }
        if (skill == glide::combat::ForcedEngagement::Skill && damage && !immune) {
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            if (!glide::combat::ForcedEngagement::Contains(profile_->Info().build, context.position,
                                                           {target.x, target.y, target.z}))
                return failed;
            WorldActionContext receiver;
            receiver.now = context.now;
            receiver.wallNow = context.wallNow;
            receiver.peerAgent = change.agent;
            receiver.healthState = change.before;
            receiver.skillEffects = extra(target, "skill_effects");
            receiver.conditionState = extra(target, "condition_effects");
            receiver.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareEngagement(receiver, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
            beforeTaunts[i] = extra(target, "taunt_state");
            glide::combat::TauntState taunt;
            if (!taunt.Load(profile_->Info().build, beforeTaunts[i]))
                return failed;
            if (!hostileEffects[i]->controlBlocked) {
                if (!taunt.Apply(change.impactUtc, context.wallNow, context.selfIdentity))
                    return failed;
                if (taunt.Active(context.wallNow)) {
                    if (nextSequence_ == UINT32_MAX)
                        return failed;
                    receiver.sequence = nextSequence_++;
                    receiver.controlImpactUtc = change.impactUtc;
                    if (found->worldActions->PrepareStun(receiver, controls[i]) != WorldActionResult::Handled ||
                        !controls[i])
                        return failed;
                }
            }
            afterTaunts[i] = taunt.Text();
        }
        if (leap && damage && !immune) {
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            if (!glide::combat::DemonLeapImpact::Contains(context.position, {target.x, target.y, target.z}))
                return failed;
            WorldActionContext receiver;
            receiver.now = context.now;
            receiver.wallNow = context.wallNow;
            receiver.peerAgent = change.agent;
            receiver.healthState = change.before;
            receiver.skillEffects = extra(target, "skill_effects");
            receiver.conditionState = extra(target, "condition_effects");
            receiver.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareDemonLeapImpact(receiver, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
        }
        if (elemental && damage && !immune) {
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            if (!glide::combat::ElementalBlast::Contains(context.position, {target.x, target.y, target.z}))
                return failed;
            WorldActionContext receiver;
            receiver.now = context.now;
            receiver.wallNow = context.wallNow;
            receiver.peerAgent = change.agent;
            receiver.healthState = change.before;
            receiver.skillEffects = extra(target, "skill_effects");
            receiver.conditionState = extra(target, "condition_effects");
            receiver.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareElementalBlast(receiver, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
        }
        if (burst && damage && !immune) {
            error = "Burst impact receiver effects could not be prepared";
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            if (!glide::combat::BurstStrengthImpact::Contains(context.position, {target.x, target.y, target.z}))
                return failed;
            WorldActionContext receiver;
            receiver.now = context.now;
            receiver.wallNow = context.wallNow;
            receiver.peerAgent = change.agent;
            receiver.healthState = change.before;
            receiver.skillEffects = extra(target, "skill_effects");
            receiver.conditionState = extra(target, "condition_effects");
            receiver.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareBurstStrength(receiver, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
            error = "Burst impact could not be committed";
        }
        if (jade && damage && !immune) {
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            if (!glide::combat::JadeWinds::Contains(context.position, {target.x, target.y, target.z}))
                return failed;
            beforeControls[i] = extra(target, "control_state");
            glide::combat::StunState control;
            if (!control.Load(profile_->Info().build, beforeControls[i]) ||
                !control.Apply(change.impactUtc, context.wallNow))
                return failed;
            WorldActionContext effects;
            effects.now = context.now;
            effects.wallNow = context.wallNow;
            effects.peerAgent = change.agent;
            effects.healthState = change.before;
            effects.skillEffects = extra(target, "skill_effects");
            effects.conditionState = extra(target, "condition_effects");
            effects.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareStability(effects, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
            const bool stable = hostileEffects[i]->controlBlocked;
            afterControls[i] = stable ? beforeControls[i] : control.Text();
            if (!stable && control.Active(context.wallNow)) {
                if (nextSequence_ == UINT32_MAX)
                    return failed;
                WorldActionContext receiver;
                receiver.now = context.now;
                receiver.wallNow = context.wallNow;
                receiver.sequence = nextSequence_++;
                receiver.peerAgent = change.agent;
                receiver.controlImpactUtc = change.impactUtc;
                if (found->worldActions->PrepareStun(receiver, controls[i]) != WorldActionResult::Handled ||
                    !controls[i])
                    return failed;
            }
        }
        if (skill == glide::combat::BanishEnchantment::Skill && damage && !immune) {
            if (!change.impactUtc || change.impactUtc > context.wallNow || !found->worldActions)
                return failed;
            const auto& target = found->activeWorld->character;
            WorldCombatTarget aim;
            if (!context.combatTarget || !context.combatTarget(targets.front().agent, aim) ||
                !glide::combat::BanishEnchantment::Contains(profile_->Info().build, context.position,
                                                            aim.actor.position, {target.x, target.y, target.z}))
                return failed;
            const double distance = 32 * std::hypot(std::hypot(double(target.x) - context.position.x,
                                                               double(target.y) - context.position.y),
                                                    double(target.z) - context.position.z);
            if (!std::isfinite(distance) || distance > glide::combat::BanishEnchantment::Range(profile_->Info().build))
                return failed;
            WorldActionContext receiver;
            receiver.now = context.now;
            receiver.wallNow = context.wallNow;
            receiver.peerAgent = change.agent;
            receiver.healthState = change.before;
            receiver.skillEffects = extra(target, "skill_effects");
            receiver.conditionState = extra(target, "condition_effects");
            receiver.removedBoons = extra(target, "removed_boons");
            if (found->worldActions->PrepareBanish(receiver, change.impactUtc, hostileEffects[i]) !=
                    WorldActionResult::Handled ||
                !hostileEffects[i])
                return failed;
        }
        if (!siphonApplied && damage && !immune && siphonReady) {
            float source = 0, target = 0;
            const auto build = profile_->Info().build;
            if (!modern::ReadHealthState(health, source, maximum) ||
                !modern::ReadHealthState(change.after, target, maximum))
                return failed;
            const float healed = (std::min)(maximum, source + glide::combat::AssassinSiphon::Healing(build, level));
            const float damaged = (std::max)(0.0f, target - glide::combat::AssassinSiphon::Damage(build, level));
            if (!siphon.Consume(context.wallNow) || !modern::WriteHealthState(healed, health, maximum) ||
                !modern::WriteHealthState(damaged, change.after, maximum))
                return failed;
            if (healed > source) {
                const auto actor = state.peerIdentity->character, local = beta::CombatLocalActor(build, actor);
                if (!beta::CombatHealthUpdate(build, local, 26937, source, healed, siphonOwned, local) ||
                    !beta::CombatHealthUpdate(build, actor, 26937, source, healed, siphonPeers, actor))
                    return failed;
            }
            siphonApplied = true;
        }
        mutexes.push_back(AccountMutex(found->session->login));
        locks.emplace_back(*mutexes.back(), std::try_to_lock);
        if (!locks.back().owns_lock()) {
            error = "Allied health account is busy";
            return failed;
        }
        float before = 0, current = 0;
        Buffer update, owned, sourceView;
        const auto sourceAgent = state.peerIdentity ? state.peerIdentity->character : uint16_t(0);
        if (!immune &&
            (!modern::ReadHealthState(change.before, before, maximum) ||
             !modern::ReadHealthState(change.after, current, maximum) ||
             !beta::CombatHealthUpdate(profile_->Info().build, change.agent, skill, before, current, update,
                                       sourceAgent) ||
             !beta::CombatHealthUpdate(profile_->Info().build,
                                       beta::CombatLocalActor(profile_->Info().build, change.agent), skill, before,
                                       current, owned, sourceAgent) ||
             !beta::CombatHealthUpdate(profile_->Info().build, change.agent, skill, before, current, sourceView,
                                       beta::CombatLocalActor(profile_->Info().build, sourceAgent))))
            return failed;
        participants.push_back(found);
        updates.push_back(std::move(update));
        ownedUpdates.push_back(std::move(owned));
        sourceUpdates.push_back(std::move(sourceView));
    }
    std::vector<KvDocument> originals, replacements;
    for (size_t i = 0; i < participants.size(); ++i) {
        auto& live = *participants[i];
        const auto& active = live.activeWorld->character;
        KvDocument document;
        AccountState account;
        if (accountIo_ && !accountIo_(AccountIo::Load, live.session->login)) {
            error = "Allied health account read rejected";
            return failed;
        }
        if (!accounts_->Load(live.session->login, document, error) || !DecodeAccountState(document, account, error))
            return failed;
        const Character* saved = nullptr;
        for (const auto& character : account.characters)
            if (character.slot == active.slot)
                saved = &character;
        if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
            saved->map != live.savedMap || field(*saved) != field(active) ||
            (i == 0 && glide::combat::AssassinSiphon::Field(*saved) != glide::combat::AssassinSiphon::Field(active))) {
            error = "Allied health saved state changed";
            return failed;
        }
        if (i && hostileEffects[i - 1]) {
            const auto& update = *hostileEffects[i - 1];
            if (extra(*saved, "skill_effects") != update.beforeEffects ||
                extra(*saved, "condition_effects") != update.beforeConditions ||
                extra(*saved, "removed_boons") != update.beforeRemoved) {
                error = "Hostile effect saved state changed";
                return failed;
            }
        }
        if (i && jade && !targets[i - 1].missed && !targets[i - 1].blocked && !targets[i - 1].evaded &&
            extra(*saved, "control_state") != beforeControls[i - 1]) {
            error = "Saved control state changed";
            return failed;
        }
        if (i && skill == glide::combat::ForcedEngagement::Skill && !targets[i - 1].missed && !targets[i - 1].blocked &&
            !targets[i - 1].evaded && extra(*saved, "taunt_state") != beforeTaunts[i - 1])
            return failed;
        if (i == 0 && blinded && extra(*saved, "condition_effects") != beforeConditions) {
            error = "Blindness saved state changed";
            return failed;
        }
        originals.push_back(document);
        if (!document.Set("character " + std::to_string(active.slot), "health_state",
                          i ? targets[i - 1].after : health))
            return failed;
        if (i == 0 && blinded &&
            !document.Set("character " + std::to_string(active.slot), "condition_effects", blind.after))
            return failed;
        if (i == 0 && siphonApplied &&
            !document.Set("character " + std::to_string(active.slot), "assassin_siphon", siphon.Text()))
            return failed;
        if (i && hostileEffects[i - 1]) {
            const auto& update = *hostileEffects[i - 1];
            const auto section = "character " + std::to_string(active.slot);
            if (!document.Set(section, "skill_effects", update.afterEffects) ||
                !document.Set(section, "condition_effects", update.afterConditions) ||
                !document.Set(section, "removed_boons", update.afterRemoved))
                return failed;
        }
        if (i && skill == glide::combat::ForcedEngagement::Skill && !targets[i - 1].missed && !targets[i - 1].blocked &&
            !targets[i - 1].evaded &&
            !document.Set("character " + std::to_string(active.slot), "taunt_state", afterTaunts[i - 1]))
            return failed;
        if (i && jade && !targets[i - 1].missed && !targets[i - 1].blocked && !targets[i - 1].evaded &&
            !document.Set("character " + std::to_string(active.slot), "control_state", afterControls[i - 1]))
            return failed;
        replacements.push_back(std::move(document));
    }
    for (const auto* live : participants)
        if (accountIo_ && !accountIo_(AccountIo::Save, live->session->login)) {
            error = "Allied health account write rejected";
            return failed;
        }
    const auto committed = accounts_->SaveBatch(originals, replacements, error);
    if (committed == AccountBatchResult::NotApplied)
        return failed;
    if (committed == AccountBatchResult::CommittedPendingRecovery) {
        for (auto& pair : sessions_)
            pair.second.outgoingOverflow = true;
        return WorldConditionCommit::RecoveryRequired;
    }
    state.activeWorld->character.profileFields["health_state"] = health;
    if (blinded)
        state.activeWorld->character.profileFields["condition_effects"] = blind.after;
    if (siphonApplied)
        state.activeWorld->character.profileFields["assassin_siphon"] = siphon.Text();
    for (size_t i = 0; i < targets.size(); ++i)
        participants[i + 1]->activeWorld->character.profileFields["health_state"] = targets[i].after;
    for (size_t i = 0; i < hostileEffects.size(); ++i)
        if (hostileEffects[i]) {
            auto& target = participants[i + 1]->activeWorld->character;
            auto& update = *hostileEffects[i];
            target.profileFields["skill_effects"] = update.afterEffects;
            target.profileFields["condition_effects"] = update.afterConditions;
            target.profileFields["removed_boons"] = update.afterRemoved;
            update.Commit();
        }
    for (size_t i = 0; i < targets.size(); ++i)
        if (skill == glide::combat::ForcedEngagement::Skill && !targets[i].missed && !targets[i].blocked &&
            !targets[i].evaded) {
            participants[i + 1]->activeWorld->character.profileFields["taunt_state"] = afterTaunts[i];
            if (controls[i])
                controls[i]->Commit();
        }
    for (size_t i = 0; i < targets.size(); ++i)
        if (jade && !targets[i].missed && !targets[i].blocked && !targets[i].evaded) {
            participants[i + 1]->activeWorld->character.profileFields["control_state"] = afterControls[i];
            if (controls[i])
                controls[i]->Commit();
        }
    for (size_t i = 0; i < targets.size(); ++i)
        if (targets[i].blocked) {
            const auto identity = state.activeWorld->character.guid.Text() + ":" + std::to_string(state.worldOwner);
            if (!participants[i + 1]->worldActions->BlockIncoming(state.peerIdentity->character, identity,
                                                                  context.now)) {
                for (auto& pair : sessions_)
                    pair.second.outgoingOverflow = true;
                return WorldConditionCommit::RecoveryRequired;
            }
        }
    for (size_t i = 0; i < hostileEffects.size(); ++i)
        if (hostileEffects[i]) {
            auto& peer = *participants[i + 1];
            auto& out = hostileEffects[i]->output;
            if (out.owned.Size())
                QueuePeerLocked(peer, std::make_shared<Buffer>(std::move(out.owned)));
            if (out.peers.Size())
                BroadcastPeerLocked(peer, std::move(out.peers), 0, Buffer(), peer.worldActions->PeerEffectVersion());
        }
    for (size_t i = 0; i < controls.size(); ++i)
        if (controls[i]) {
            auto& peer = *participants[i + 1];
            auto& output = controls[i]->output;
            if (output.owned.Size())
                QueuePeerLocked(peer, std::make_shared<Buffer>(std::move(output.owned)));
            if (output.peers.Size())
                BroadcastPeerLocked(peer, std::move(output.peers));
        }
    if (skill == glide::combat::ForcedEngagement::Skill)
        for (size_t i = 0; i < targets.size(); ++i)
            if (!RefreshTauntLocked(*participants[i + 1])) {
                for (auto& pair : sessions_)
                    pair.second.outgoingOverflow = true;
                return WorldConditionCommit::RecoveryRequired;
            }
    if (siphonOwned.Size())
        QueuePeerLocked(state, std::make_shared<Buffer>(std::move(siphonOwned)));
    if (siphonPeers.Size())
        BroadcastPeerLocked(state, std::move(siphonPeers));
    for (size_t i = 0; i < updates.size(); ++i) {
        if (targets[i].missed || targets[i].blocked || targets[i].evaded)
            continue;
        auto& peer = *participants[i + 1];
        QueuePeerLocked(peer, std::make_shared<Buffer>(std::move(ownedUpdates[i])));
        BroadcastPeerLocked(peer, std::move(updates[i]), state.peerIdentity->character, std::move(sourceUpdates[i]));
    }
    error.clear();
    return WorldConditionCommit::Applied;
}
WorldConditionCommit Server::SaveHostileEffectsLocked(SessionState& state, const WorldActionContext& c, unsigned skill,
                                                      uint64_t nominal, const std::string& deadlines,
                                                      const std::string& effects,
                                                      const std::vector<WorldHostileTarget>& targets,
                                                      std::string& error) {
    const auto failed = WorldConditionCommit::NotApplied;
    error = "Hostile effects could not be prepared";
    if (!accounts_ || !profile_ || skill != combat::GazeDarkness::Skill ||
        !combat::GazeDarkness::Build(profile_->Info().build) || !profile_->SupportsConditionStatePersistence() ||
        !profile_->SupportsSkillStatePersistence() || targets.size() > combat::GazeDarkness::MaximumTargets ||
        deadlines.size() > 1024 || effects.size() > 8192)
        return failed;
    const auto ready = [](const SessionState& s) {
        return s.session && !s.loggedOut && s.worldOwner && s.worldReady && s.peerAnnounced && s.activeWorld &&
               s.peerIdentity && s.worldActions && !s.outgoingOverflow;
    };
    const auto field = [](const Character& ch, const char* name) {
        const auto it = ch.profileFields.find(name);
        return it == ch.profileFields.end() ? std::string() : it->second;
    };
    if (!ready(state) || field(state.activeWorld->character, "health_state") != c.healthState ||
        field(state.activeWorld->character, "skill_effects") != c.skillEffects)
        return failed;
    float casterHealth = 0;
    const auto maximum = beta::CombatMaximumHealth(profile_->Info().build);
    if (!modern::ReadHealthState(c.healthState, casterHealth, maximum) || casterHealth <= 0 ||
        (!state.combatEnabled && !targets.empty()))
        return failed;
    std::vector<WorldCombatTarget> expected;
    if (!combat::GazeDarkness::Select(profile_->Info().build, c, nominal, expected) ||
        expected.size() != targets.size())
        return failed;
    std::vector<SessionState*> participants{&state};
    std::vector<std::unique_ptr<PreparedHostileEffects>> prepared;
    std::vector<std::string> beforeReveal, afterReveal;
    std::vector<std::shared_ptr<std::mutex>> mutexes;
    std::vector<std::unique_lock<std::mutex>> locks;
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        if (target.agent != expected[i].actor.agent || target.identity != expected[i].actor.identity)
            return failed;
        SessionState* found = nullptr;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (&peer != &state && ready(peer) && state.combatEnabled && peer.combatEnabled &&
                peer.peerIdentity->character == target.agent &&
                peer.activeWorld->character.map == state.activeWorld->character.map &&
                peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner) == target.identity) {
                if (found)
                    return failed;
                found = &peer;
            }
        }
        if (!found)
            return failed;
        mutexes.push_back(AccountMutex(found->session->login));
        locks.emplace_back(*mutexes.back(), std::try_to_lock);
        if (!locks.back().owns_lock()) {
            error = "Hostile effect account busy";
            return failed;
        }
        const auto& ch = found->activeWorld->character;
        WorldActionContext recipient;
        recipient.now = c.now;
        recipient.wallNow = c.wallNow;
        recipient.peerAgent = target.agent;
        recipient.position = {ch.x, ch.y, ch.z};
        recipient.healthState = field(ch, "health_state");
        recipient.skillEffects = field(ch, "skill_effects");
        recipient.conditionState = field(ch, "condition_effects");
        recipient.removedBoons = field(ch, "removed_boons");
        std::unique_ptr<PreparedHostileEffects> next;
        if (found->worldActions->PrepareGazeDarkness(recipient, nominal, next) != WorldActionResult::Handled || !next ||
            next->beforeEffects != recipient.skillEffects || next->beforeConditions != recipient.conditionState ||
            next->beforeRemoved != recipient.removedBoons || next->afterEffects.size() > 8192 ||
            next->afterConditions.size() > 8192 || next->afterRemoved.size() > 8192 || next->output.sequences ||
            next->output.targetPeerAgent || next->output.targetView.Size() || next->output.relocate ||
            next->output.flight != FlightEvent::None)
            return failed;
        combat::RevealState reveal;
        const auto before = combat::RevealState::Field(ch);
        if (!reveal.Load(profile_->Info().build, before) || !reveal.Apply(nominal, c.wallNow))
            return failed;
        beforeReveal.push_back(before);
        afterReveal.push_back(reveal.Text());
        participants.push_back(found);
        prepared.push_back(std::move(next));
    }
    std::vector<KvDocument> originals, replacements;
    for (size_t i = 0; i < participants.size(); ++i) {
        auto& live = *participants[i];
        const auto& active = live.activeWorld->character;
        KvDocument doc;
        AccountState account;
        if (accountIo_ && !accountIo_(AccountIo::Load, live.session->login)) {
            error = "Hostile effect account read rejected";
            return failed;
        }
        if (!accounts_->Load(live.session->login, doc, error) || !DecodeAccountState(doc, account, error))
            return failed;
        const Character* saved = nullptr;
        for (const auto& ch : account.characters)
            if (ch.slot == active.slot)
                saved = &ch;
        if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
            saved->map != live.savedMap)
            return failed;
        for (const char* name : {"health_state", "skill_effects", "condition_effects", "removed_boons", "reveal_state"})
            if (field(*saved, name) != field(active, name))
                return failed;
        if (!i && field(*saved, "skill_recharges") != field(active, "skill_recharges"))
            return failed;
        if (i && (field(active, "skill_effects") != prepared[i - 1]->beforeEffects ||
                  field(active, "condition_effects") != prepared[i - 1]->beforeConditions ||
                  field(active, "removed_boons") != prepared[i - 1]->beforeRemoved ||
                  combat::RevealState::Field(active) != beforeReveal[i - 1]))
            return failed;
        originals.push_back(doc);
        const auto section = "character " + std::to_string(active.slot);
        if (!i) {
            if (!doc.Set(section, "skill_effects", effects) || !doc.Set(section, "skill_recharges", deadlines))
                return failed;
        } else if (!doc.Set(section, "skill_effects", prepared[i - 1]->afterEffects) ||
                   !doc.Set(section, "condition_effects", prepared[i - 1]->afterConditions) ||
                   !doc.Set(section, "removed_boons", prepared[i - 1]->afterRemoved) ||
                   !doc.Set(section, "reveal_state", afterReveal[i - 1]))
            return failed;
        replacements.push_back(std::move(doc));
    }
    for (const auto* live : participants)
        if (accountIo_ && !accountIo_(AccountIo::Save, live->session->login)) {
            error = "Hostile effect account write rejected";
            return failed;
        }
    const auto saved = accounts_->SaveBatch(originals, replacements, error);
    if (saved == AccountBatchResult::NotApplied)
        return failed;
    if (saved == AccountBatchResult::CommittedPendingRecovery) {
        for (auto& pair : sessions_)
            pair.second.outgoingOverflow = true;
        return WorldConditionCommit::RecoveryRequired;
    }
    auto& caster = state.activeWorld->character;
    caster.profileFields["skill_effects"] = effects;
    caster.profileFields["skill_recharges"] = deadlines;
    for (size_t i = 0; i < prepared.size(); ++i) {
        auto& ch = participants[i + 1]->activeWorld->character;
        auto& next = *prepared[i];
        ch.profileFields["skill_effects"] = next.afterEffects;
        ch.profileFields["condition_effects"] = next.afterConditions;
        ch.profileFields["removed_boons"] = next.afterRemoved;
        ch.profileFields["reveal_state"] = afterReveal[i];
        next.Commit();
    }
    for (size_t i = 0; i < prepared.size(); ++i) {
        auto& peer = *participants[i + 1];
        auto& output = prepared[i]->output;
        if (output.owned.Size())
            QueuePeerLocked(peer, std::make_shared<Buffer>(std::move(output.owned)));
        if (output.peers.Size())
            BroadcastPeerLocked(peer, std::move(output.peers), 0, Buffer(), peer.worldActions->PeerEffectVersion());
        if (peer.outgoingOverflow) {
            for (auto& pair : sessions_)
                pair.second.outgoingOverflow = true;
            return WorldConditionCommit::RecoveryRequired;
        }
    }
    for (size_t i = 1; i < participants.size(); ++i)
        if (!RefreshRevealLocked(*participants[i])) {
            for (auto& pair : sessions_)
                pair.second.outgoingOverflow = true;
            return WorldConditionCommit::RecoveryRequired;
        }
    error.clear();
    return WorldConditionCommit::Applied;
}
WorldConditionCommit
Server::SaveAlliedEffectsLocked(SessionState& state, const WorldActionContext& context, const std::string& deadlines,
                                const std::string& effects, const std::vector<WorldReceivedEffectTarget>& targets,
                                std::vector<unsigned>& instances, std::string& error,
                                const std::function<bool(const std::vector<unsigned>&, std::string&)>& prepareOwn) {
    const auto failed = WorldConditionCommit::NotApplied;
    error = "Allied effects could not be prepared";
    if (!accounts_ || !profile_ || !profile_->SupportsSkillStatePersistence() || targets.size() > 25 ||
        deadlines.size() > 1024 || effects.size() > 8192 || !context.wallNow)
        return failed;
    const auto ready = [](const SessionState& value) {
        return value.session && !value.loggedOut && value.worldOwner && value.worldReady && value.activeWorld &&
               value.peerIdentity && value.worldActions && !value.outgoingOverflow;
    };
    if (!ready(state))
        return failed;
    const auto field = [](const Character& value, const char* key) {
        const auto at = value.profileFields.find(key);
        return at == value.profileFields.end() ? std::string() : at->second;
    };
    std::vector<SessionState*> participants{&state};
    std::vector<std::shared_ptr<std::mutex>> mutexes;
    std::vector<std::unique_lock<std::mutex>> locks;
    std::vector<std::unique_ptr<PreparedWorldEffect>> prepared;
    // Prepare one copy per account, even when several facets pulse together.
    std::vector<std::vector<size_t>> groups;
    for (size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        if (!target.agent || target.endsUtc <= context.wallNow ||
            !modern::AllowedReceivedEffect(profile_->Info().build, target.definition, target.endsUtc - context.wallNow))
            return failed;
        size_t group = 0;
        for (; group < groups.size(); ++group)
            if (targets[groups[group].front()].agent == target.agent)
                break;
        if (group == groups.size()) {
            if (groups.size() == 5)
                return failed;
            groups.push_back({i});
        } else {
            if (groups[group].size() == 5 || targets[groups[group].front()].identity != target.identity)
                return failed;
            for (const auto at : groups[group])
                if (targets[at].definition == target.definition ||
                    (target.refreshInstance && targets[at].refreshInstance == target.refreshInstance))
                    return failed;
            groups[group].push_back(i);
        }
    }
    for (const auto& group : groups) {
        const auto& target = targets[group.front()];
        SessionState* found = nullptr;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (&peer != &state && ready(peer) && peer.peerAnnounced && peer.peerIdentity->character == target.agent &&
                peer.activeWorld->character.map == state.activeWorld->character.map &&
                !(state.combatEnabled && peer.combatEnabled) &&
                peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner) == target.identity)
                found = &peer;
        }
        if (!found)
            return failed;
        float health = 0;
        if (!modern::ReadHealthState(found->activeWorld->character, health) || health <= 0)
            return failed;
        mutexes.push_back(AccountMutex(found->session->login));
        locks.emplace_back(*mutexes.back(), std::try_to_lock);
        if (!locks.back().owns_lock()) {
            error = "Allied effect account is busy";
            return failed;
        }
        const auto& character = found->activeWorld->character;
        WorldActionContext recipient;
        recipient.now = context.now;
        recipient.wallNow = context.wallNow;
        recipient.selfIdentity = target.identity;
        recipient.sequence = context.sequence;
        recipient.peerAgent = target.agent;
        recipient.position = {character.x, character.y, character.z};
        recipient.healthState = field(character, "health_state");
        recipient.conditionState = field(character, "condition_effects");
        recipient.skillEffects = field(character, "skill_effects");
        recipient.removedBoons = field(character, "removed_boons");
        std::vector<ReceivedWorldEffect> requests;
        for (const auto at : group) {
            ReceivedWorldEffect request;
            request.definition = targets[at].definition;
            request.refreshInstance = targets[at].refreshInstance;
            request.endsUtc = targets[at].endsUtc;
            // Attribute regeneration to the authenticated emitter, not the receiver
            // or caller-supplied target metadata. Existing effects keep zero.
            if (request.definition == 718 ||
                (beta::BetaHealthBuild(profile_->Info().build) && request.definition == 289))
                request.sourceLevel = state.activeWorld->character.level;
            requests.push_back(request);
        }
        std::unique_ptr<PreparedWorldEffect> candidate;
        if (found->worldActions->PrepareReceivedEffects(recipient, requests, candidate) != WorldActionResult::Handled ||
            !candidate || candidate->instances.size() != requests.size() ||
            candidate->before != recipient.skillEffects || candidate->after.size() > 8192 ||
            candidate->output.sequences || candidate->output.targetPeerAgent || candidate->output.targetView.Size() ||
            candidate->output.relocate || candidate->output.flight != FlightEvent::None)
            return failed;
        std::set<unsigned> identifiers;
        for (const auto instance : candidate->instances)
            if (!instance || !identifiers.insert(instance).second)
                return failed;
        participants.push_back(found);
        prepared.push_back(std::move(candidate));
    }
    std::vector<unsigned> allocated(targets.size());
    for (size_t i = 0; i < prepared.size(); ++i)
        for (size_t j = 0; j < groups[i].size(); ++j)
            allocated[groups[i][j]] = prepared[i]->instances[j];
    std::string casterEffects = effects;
    if (prepareOwn && (!prepareOwn(allocated, casterEffects) || casterEffects.size() > 8192))
        return failed;
    std::vector<KvDocument> originals, replacements;
    for (size_t i = 0; i < participants.size(); ++i) {
        auto& live = *participants[i];
        const auto& active = live.activeWorld->character;
        KvDocument document;
        AccountState account;
        if (accountIo_ && !accountIo_(AccountIo::Load, live.session->login)) {
            error = "Allied effect account read rejected";
            return failed;
        }
        if (!accounts_->Load(live.session->login, document, error) || !DecodeAccountState(document, account, error))
            return failed;
        error = "Allied effect saved state changed";
        const Character* saved = nullptr;
        for (const auto& character : account.characters)
            if (character.slot == active.slot)
                saved = &character;
        if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
            saved->map != live.savedMap || field(*saved, "skill_effects") != field(active, "skill_effects") ||
            (i && field(active, "skill_effects") != prepared[i - 1]->before) ||
            (!i && field(*saved, "skill_recharges") != field(active, "skill_recharges")))
            return failed;
        originals.push_back(document);
        const auto section = "character " + std::to_string(active.slot);
        if (!document.Set(section, "skill_effects", i ? prepared[i - 1]->after : casterEffects) ||
            (!i && !document.Set(section, "skill_recharges", deadlines)))
            return failed;
        replacements.push_back(std::move(document));
    }
    for (const auto* live : participants)
        if (accountIo_ && !accountIo_(AccountIo::Save, live->session->login)) {
            error = "Allied effect account write rejected";
            return failed;
        }
    const auto committed = accounts_->SaveBatch(originals, replacements, error);
    if (committed == AccountBatchResult::NotApplied)
        return failed;
    if (committed == AccountBatchResult::CommittedPendingRecovery) {
        for (auto& pair : sessions_)
            pair.second.outgoingOverflow = true;
        return WorldConditionCommit::RecoveryRequired;
    }
    state.activeWorld->character.profileFields["skill_effects"] = casterEffects;
    state.activeWorld->character.profileFields["skill_recharges"] = deadlines;
    for (size_t i = 0; i < prepared.size(); ++i) {
        auto& update = *prepared[i];
        auto& peer = *participants[i + 1];
        peer.activeWorld->character.profileFields["skill_effects"] = update.after;
        update.Commit();
    }
    // Every recipient cache is current before any observer can receive a batch.
    for (size_t i = 0; i < prepared.size(); ++i) {
        auto& output = prepared[i]->output;
        auto& peer = *participants[i + 1];
        if (output.owned.Size())
            QueuePeerLocked(peer, std::make_shared<Buffer>(std::move(output.owned)));
        if (output.peers.Size())
            BroadcastPeerLocked(peer, std::move(output.peers), 0, Buffer(), peer.worldActions->PeerEffectVersion());
    }
    instances = std::move(allocated);
    error.clear();
    return WorldConditionCommit::Applied;
}
WorldConditionCommit Server::SaveConditionTransferLocked(SessionState& state, const std::string& deadlines,
                                                         const std::string& effects, const std::string& health,
                                                         const std::string& conditions,
                                                         const std::vector<WorldConditionChange>& donors,
                                                         std::string& error) {
    error = "Condition transfer could not be prepared";
    const auto failed = WorldConditionCommit::NotApplied;
    if (!accounts_ || !profile_ || !characterCodec_ || !profile_->SupportsConditionStatePersistence() ||
        !profile_->SupportsHealthStatePersistence() || !profile_->SupportsSkillStatePersistence() ||
        donors.size() > 4 || deadlines.size() > 1024 || effects.size() > 8192 || health.size() > 12 ||
        conditions.size() > 8192)
        return failed;
    const auto ready = [](const SessionState& value) {
        return value.session && !value.loggedOut && value.worldOwner && value.worldReady && value.activeWorld &&
               value.peerIdentity && !value.outgoingOverflow;
    };
    if (!ready(state))
        return failed;
    float ownHealth = 0;
    if (!modern::ReadHealthState(health, ownHealth) || ownHealth <= 0)
        return failed;
    const auto validConditions = [&](const std::string& text) {
        switch (profile_->Info().build) {
        case 14075:
        case 14422:
        case 14505:
        case 14812: {
            modern::ConditionState<14505> parsed;
            return parsed.Load(text);
        }
        case 49284: {
            modern::ConditionState<49284> parsed;
            return parsed.Load(text);
        }
        case 167409: {
            modern::ConditionState<167409> parsed;
            return parsed.Load(text);
        }
        case 204489: {
            modern::ConditionState<204489> parsed;
            return parsed.Load(text);
        }
        default:
            return false;
        }
    };
    if (!validConditions(conditions))
        return failed;
    std::vector<SessionState*> participants{&state};
    std::set<uint16_t> agents;
    std::vector<std::shared_ptr<std::mutex>> mutexes;
    std::vector<std::unique_lock<std::mutex>> locks;
    for (const auto& change : donors) {
        if (!change.agent || !agents.insert(change.agent).second || change.before.size() > 8192 ||
            change.after.size() > 8192 || !validConditions(change.after))
            return failed;
        SessionState* found = nullptr;
        for (auto& pair : sessions_) {
            auto& peer = pair.second;
            if (&peer != &state && ready(peer) && peer.peerAnnounced && peer.peerIdentity->character == change.agent &&
                peer.activeWorld->character.map == state.activeWorld->character.map &&
                !(state.combatEnabled && peer.combatEnabled) &&
                peer.activeWorld->character.guid.Text() + ":" + std::to_string(peer.worldOwner) == change.identity)
                found = &peer;
        }
        if (!found)
            return failed;
        float targetHealth = 0;
        if (!modern::ReadHealthState(found->activeWorld->character, targetHealth) || targetHealth <= 0)
            return failed;
        // Never wait for another account while holding stateMutex_: its owner
        // may itself be waiting for stateMutex_. Busy means no operation applied.
        mutexes.push_back(AccountMutex(found->session->login));
        locks.emplace_back(*mutexes.back(), std::try_to_lock);
        if (!locks.back().owns_lock()) {
            error = "Condition transfer account is busy";
            return failed;
        }
        participants.push_back(found);
    }
    const auto field = [](const Character& value, const char* key) {
        const auto at = value.profileFields.find(key);
        return at == value.profileFields.end() ? std::string() : at->second;
    };
    std::vector<KvDocument> originals, replacements;
    for (size_t i = 0; i < participants.size(); ++i) {
        auto& live = *participants[i];
        const auto& active = live.activeWorld->character;
        KvDocument document;
        AccountState account;
        if (accountIo_ && !accountIo_(AccountIo::Load, live.session->login)) {
            error = "Condition transfer account read rejected";
            return failed;
        }
        if (!accounts_->Load(live.session->login, document, error) || !DecodeAccountState(document, account, error))
            return failed;
        error = "Condition transfer saved state changed";
        const Character* saved = nullptr;
        for (const auto& candidate : account.characters)
            if (candidate.slot == active.slot)
                saved = &candidate;
        if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
            saved->map != live.savedMap)
            return failed;
        if (field(*saved, "condition_effects") != field(active, "condition_effects"))
            return failed;
        if (i && field(active, "condition_effects") != donors[i - 1].before)
            return failed;
        originals.push_back(document);
        const auto section = "character " + std::to_string(active.slot);
        if (!document.Set(section, "condition_effects", i ? donors[i - 1].after : conditions))
            return failed;
        if (!i) {
            for (const auto* key : {"skill_recharges", "skill_effects", "health_state"})
                if (field(*saved, key) != field(active, key))
                    return failed;
            if (!document.Set(section, "skill_recharges", deadlines) ||
                !document.Set(section, "skill_effects", effects) || !document.Set(section, "health_state", health))
                return failed;
        }
        replacements.push_back(std::move(document));
    }
    // Failure injection happens before the durable commit, just like any other
    // preflight failure. AccountFiles owns crash recovery after that boundary.
    for (const auto* live : participants)
        if (accountIo_ && !accountIo_(AccountIo::Save, live->session->login)) {
            error = "Condition transfer account write rejected";
            return failed;
        }
    const auto committed = accounts_->SaveBatch(originals, replacements, error);
    if (committed == AccountBatchResult::NotApplied)
        return failed;
    if (committed == AccountBatchResult::CommittedPendingRecovery) {
        // The store is unavailable until recovery; prevent every connected
        // account from continuing with stale in-memory state.
        for (auto& pair : sessions_)
            pair.second.outgoingOverflow = true;
        return WorldConditionCommit::RecoveryRequired;
    }
    for (size_t i = 0; i < participants.size(); ++i)
        participants[i]->activeWorld->character.profileFields["condition_effects"] =
            i ? donors[i - 1].after : conditions;
    auto& own = state.activeWorld->character.profileFields;
    own["skill_recharges"] = deadlines;
    own["skill_effects"] = effects;
    own["health_state"] = health;
    error.clear();
    return WorldConditionCommit::Applied;
}
bool Server::SaveWorldSkillStateLocked(SessionState& activeState, const std::string& deadlines,
                                       const std::string& effects, std::string& error, bool releaseState,
                                       const std::string* health, uint8_t professionState,
                                       const std::string* conditions, const std::string* legends,
                                       const std::string* endurance, const std::string* initiative,
                                       const std::string* siphon, bool stunBreak, const std::string* conditionClock) {
    error = "Cannot save skill state for this connection";
    if (!accounts_ || !characterCodec_ || !profile_ || !profile_->SupportsSkillStatePersistence() ||
        !activeState.session || activeState.loggedOut || !activeState.worldOwner || !activeState.worldReady ||
        !activeState.activeWorld || deadlines.size() > 1024 || effects.size() > 8192 || professionState > 4 ||
        (professionState && !profile_->SupportsProfessionStatePersistence()) ||
        (health && (!profile_->SupportsHealthStatePersistence() || health->size() > 12)) ||
        (conditions && (!profile_->SupportsConditionStatePersistence() || conditions->size() > 8192)) ||
        (legends && (!profile_->SupportsLegendStatePersistence() || legends->size() > 32)))
        return false;
    if (conditionClock && !combat::ConditionDamage::ValidClock(profile_->Info().build, *conditionClock))
        return false;
    auto& active = activeState.activeWorld->character;
    KvDocument document;
    AccountState account;
    if (stunBreak) {
        glide::combat::StunState control;
        const auto profession = active.profileFields.find("profession");
        if (profession == active.profileFields.end() || profession->second != "9" ||
            !control.Load(profile_->Info().build, glide::combat::StunState::Field(active)))
            return false;
        glide::combat::TauntState taunt;
        if (!taunt.Load(profile_->Info().build, glide::combat::TauntState::Field(active)) ||
            (!control.Active(wallClock_()) && !taunt.Active(wallClock_())))
            return false;
    }
    if (endurance) {
        modern::EnduranceLedger resource;
        const auto profession = active.profileFields.find("profession");
        if ((profile_->Info().build != 49284 && profile_->Info().build != 167409 && profile_->Info().build != 204489) ||
            profession == active.profileFields.end() || profession->second.size() != 1 || profession->second[0] < '1' ||
            profession->second[0] > '9' || endurance->empty() || !resource.Load(*endurance))
            return false;
    }
    if (initiative) {
        auto resource = modern::InitiativeLedger::ForBuild(profile_->Info().build);
        const auto profession = active.profileFields.find("profession");
        if (!modern::InitiativeBuild(profile_->Info().build) || profession == active.profileFields.end() ||
            profession->second != "5" || initiative->empty() || !resource.Load(*initiative))
            return false;
    }
    if (siphon) {
        Character candidate = active;
        candidate.profileFields["assassin_siphon"] = *siphon;
        glide::combat::AssassinSiphon value;
        if (!value.Load(profile_->Info().build, candidate))
            return false;
    }
    if (legends && !ValidWorldLegends(profile_->Info().build, active, *legends))
        return false;
    if (professionState && (profile_->Info().build == 14422 || profile_->Info().build == 14505)) {
        const auto profession = active.profileFields.find("profession");
        Character candidate = active;
        candidate.profileFields["profession_state"] = std::to_string(professionState);
        Buffer summary;
        if (profession == active.profileFields.end() || profession->second != "6" ||
            !characterCodec_->Summary(candidate, summary)) {
            error = "Invalid June elemental selection";
            return false;
        }
    }
    if (!(releaseState ? LoadAccountLocked(activeState.session->login, document, error)
                       : accounts_->Load(activeState.session->login, document, error)) ||
        !DecodeAccountState(document, account, error))
        return false;
    const Character* saved = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == active.slot)
            saved = &character;
    if (!saved || account.selected != active.slot || saved->guid.Text() != active.guid.Text() ||
        saved->map != activeState.savedMap)
        return false;
    const auto current = [](const Character& character, const char* key) {
        const auto at = character.profileFields.find(key);
        return at == character.profileFields.end() ? std::string() : at->second;
    };
    if (current(*saved, "skill_recharges") != current(active, "skill_recharges") ||
        current(*saved, "skill_effects") != current(active, "skill_effects") ||
        (health && current(*saved, "health_state") != current(active, "health_state")) ||
        (conditions && current(*saved, "condition_effects") != current(active, "condition_effects")) ||
        (legends && current(*saved, "legend_state") != current(active, "legend_state")) ||
        (endurance && current(*saved, "endurance_state") != current(active, "endurance_state")) ||
        (initiative && current(*saved, "initiative_state") != current(active, "initiative_state")) ||
        (siphon && current(*saved, "assassin_siphon") != current(active, "assassin_siphon")) ||
        (conditionClock && current(*saved, "condition_damage_clock") != current(active, "condition_damage_clock")) ||
        (stunBreak && (current(*saved, "control_state") != current(active, "control_state") ||
                       current(*saved, "taunt_state") != current(active, "taunt_state"))) ||
        (professionState && current(*saved, "profession_state") != current(active, "profession_state"))) {
        error = "Saved skill state changed during world session";
        return false;
    }
    const std::string section = "character " + std::to_string(active.slot);
    if (!document.Set(section, "skill_recharges", deadlines) || !document.Set(section, "skill_effects", effects) ||
        (health && !document.Set(section, "health_state", *health)) ||
        (conditions && !document.Set(section, "condition_effects", *conditions)) ||
        (legends && !document.Set(section, "legend_state", *legends)) ||
        (endurance && !document.Set(section, "endurance_state", *endurance)) ||
        (initiative && !document.Set(section, "initiative_state", *initiative)) ||
        (siphon && !document.Set(section, "assassin_siphon", *siphon)) ||
        (conditionClock && !document.Set(section, "condition_damage_clock", *conditionClock)) ||
        (stunBreak && (!document.Set(section, "control_state", "") || !document.Set(section, "taunt_state", ""))) ||
        (professionState && !document.Set(section, "profession_state", std::to_string(professionState))) ||
        !(releaseState ? SaveAccountLocked(document, error) : accounts_->Save(document, error)))
        return false;
    active.profileFields["skill_recharges"] = deadlines;
    active.profileFields["skill_effects"] = effects;
    if (health)
        active.profileFields["health_state"] = *health;
    if (conditions)
        active.profileFields["condition_effects"] = *conditions;
    if (legends)
        active.profileFields["legend_state"] = *legends;
    if (endurance)
        active.profileFields["endurance_state"] = *endurance;
    if (initiative)
        active.profileFields["initiative_state"] = *initiative;
    if (siphon)
        active.profileFields["assassin_siphon"] = *siphon;
    if (conditionClock)
        active.profileFields["condition_damage_clock"] = *conditionClock;
    if (stunBreak) {
        active.profileFields["control_state"].clear();
        active.profileFields["taunt_state"].clear();
    }
    if (professionState)
        active.profileFields["profession_state"] = std::to_string(professionState);
    error.clear();
    return true;
}
} // namespace glide
