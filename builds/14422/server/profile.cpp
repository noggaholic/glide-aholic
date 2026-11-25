#include "profile.h"
#include "../build.h"
#include "auth_flow.h"
#include "character_catalog.h"
#include "create.h"
#include "bootstrap.h"
#include "game.h"
#include "game_wire.h"
#include "../../14505/server/profile.h"
#include "../../14505/server/world_skills.h"
#include "../../14505/server/inventory_state.h"
#include "glide/files.h"
#include <algorithm>

namespace glide {
namespace {
class June7Profile final : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14422;
    }
    CapabilitySet Capabilities() const override {
        return June14505Profile().Capabilities();
    }
    bool GlobalPeerIds() const override {
        return true;
    }
    bool SupportsInventoryPersistence() const override {
        return true;
    }
    bool SupportsSkillStatePersistence() const override {
        return true;
    }
    bool SupportsHealthStatePersistence() const override {
        return true;
    }
    bool SupportsConditionStatePersistence() const override {
        return true;
    }
    bool SupportsProfessionStatePersistence() const override {
        return true;
    }
    bool DefaultMapArrival(unsigned map, SpawnPoint& out) const override {
        if (map != 18)
            return false;
        // June11 waypoint1076: both builds publish MapDef28, file9521/archive191265
        // with identical bounds. June7 has no published waypoint for this map.
        out = {18, 3167.82007f / 32, -1375.32996f / 32, 5386.1001f / 32};
        return true;
    }
    // The retained server explicitly routes both June builds through this protocol.
    const StsVariant& Sts() const override {
        return June14505Profile().Sts();
    }
    const TransportVariant& Transport() const override {
        return June14505Profile().Transport();
    }
    // Shared fail-closed legacy getters; data-backed factories below are mandatory.
    const AuthCodec& Auth() const override {
        return June14505Profile().Auth();
    }
    const CreateFlow& Create() const override {
        return June14505Profile().Create();
    }
    const GameBootstrap& Bootstrap() const override {
        return June14505Profile().Bootstrap();
    }
    const WireMap& Wire() const override {
        return June14505Profile().Wire();
    }
    const GameMessages& Game() const override {
        return june14422::ServiceMessages();
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        return june14422::LoadCharacterCodec(data, out, error);
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        return june14422::CreateAuthFlow(data, out, error);
    }
    bool LoadCreateFlow(const std::wstring& data, std::unique_ptr<CreateFlow>& out, std::string& error) const override {
        // Published own waypoint 1417; native entry and saved reentry are verified.
        return june14422::CreateCharacterFlow(data, {50, 3770.72998f / 32, 6364.66992f / 32, 1194.47998f / 32}, out,
                                              error);
    }
    bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return june14422::LoadBootstrap(data, out, error);
    }
    bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) const override {
        return june14422::CreateGameWire(login, out, error);
    }
    bool CreateGameMessages(const BootstrapContext& context, std::shared_ptr<const GameMessages>& out,
                            std::string& error) const override {
        if (!Valid(context) || !context.serverPort) {
            error = "Invalid June 7 reservation or server port";
            return false;
        }
        auto candidate = june14422::MakeGameMessages(context.identity->character);
        if (!candidate) {
            error = "Invalid June 7 agent";
            return false;
        }
        out = std::move(candidate);
        error.clear();
        return true;
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (!Valid(context)) {
            error = "Invalid June 7 character reservation";
            return false;
        }
        june14422::CharacterCatalog characters;
        june14505::SkillCatalog skills(14422);
        if (!characters.LoadData(data, error) || !characters.LoadWeapons(data + L"/14422/weapon-loadouts.txt", error) ||
            !skills.Load(data + L"/14422/skill-catalog.txt", error))
            return false;
        const auto& saved = context.entry.character;
        const auto field = [&](const char* name, unsigned maximum, unsigned& value) {
            const auto at = saved.profileFields.find(name);
            return at != saved.profileFields.end() && ParseUnsigned(at->second, maximum, value);
        };
        unsigned race = 0, sex = 0, profession = 0;
        if (!field("race", 4, race) || !field("sex", 1, sex) || !field("profession", 8, profession)) {
            error = "Invalid June 7 skill loadout fields";
            return false;
        }
        const auto* recipe = characters.FindForCharacter(saved);
        if (!recipe) {
            error = "Missing June 7 skill loadout";
            return false;
        }
        std::unique_ptr<june14505::WorldSkills> actions(
            new june14505::WorldSkills(std::move(skills), recipe->loadout, context.identity->character));
        if (!actions->LoadRecharges(saved, error))
            return false;
        return june14505::CreateInventoryActions(characters, saved, context.identity->player,
                                                 context.identity->character, std::move(actions), out, error);
    }

  private:
    bool Valid(const BootstrapContext& context) const {
        const auto* id = context.identity;
        return context.catalog.build == 14422 && id && id->player && id->player < 255 &&
               id->character == 256u + id->player && !id->firstBag && !id->itemBase &&
               std::any_of(
                   context.entry.character.guid.bytes, context.entry.character.guid.bytes + 16,
                   [](uint8_t byte) { return byte != 0; });
    }
};
} // namespace
const BuildProfile& June14422Profile() {
    static June7Profile profile;
    return profile;
}
} // namespace glide
