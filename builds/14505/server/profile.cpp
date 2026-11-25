#include "glide/profile/unloaded.h"
#include "profile.h"
#include "../build.h"
#include "auth_flow.h"
#include "character_catalog.h"
#include "create.h"
#include "bootstrap.h"
#include "game.h"
#include "game_wire.h"
#include "world_skills.h"
#include "inventory_state.h"
#include "glide/files.h"

namespace glide {
namespace {
// June uses loaded codecs and per-connection AuthFlow. These legacy entry points
// must never substitute another build's messages if the required setup is absent.
class JuneProfile final : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14505;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::LocalTeleport, Capability::MapTransfer, Capability::TimeOfDay,
                                  Capability::Inspect, Capability::Announce, Capability::Markers});
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
    const StsVariant& Sts() const override {
        static const char* const emptyReplies[] = {"/Session/CreateSession"};
        static const StsVariant variant{true, "cligate.101.ncplatform.net", false, emptyReplies, 1,
                                        1002, StsLoginMode::TlsPassword};
        return variant;
    }
    const TransportVariant& Transport() const override {
        static const TransportVariant variant{101, false};
        return variant;
    }
    const AuthCodec& Auth() const override {
        static UnloadedAuth unavailable;
        return unavailable;
    }
    const CreateFlow& Create() const override {
        static UnloadedCreate unavailable;
        return unavailable;
    }
    const GameBootstrap& Bootstrap() const override {
        static UnloadedBootstrap unavailable;
        return unavailable;
    }
    const WireMap& Wire() const override {
        static june14505::GameWire unbound("");
        return unbound;
    }
    const GameMessages& Game() const override {
        return june14505::ServiceMessages();
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        return june14505::LoadCharacterCodec(data, out, error);
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        return june14505::CreateAuthFlow(data, out, error);
    }
    bool LoadCreateFlow(const std::wstring& data, std::unique_ptr<CreateFlow>& out, std::string& error) const override {
        // Waypoint 1417 provides a ground-level starting position for this client.
        return june14505::CreateCharacterFlow(data, {50, 3770.72998f / 32, 6364.66992f / 32, 1194.47998f / 32}, out,
                                              error);
    }
    bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return june14505::LoadBootstrap(data, out, error);
    }
    bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) const override {
        return june14505::CreateGameWire(login, out, error);
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        const auto* id = context.identity;
        if (context.catalog.build != 14505 || !id || !id->player || id->player == 255 ||
            id->character != 0x100u + id->player || id->firstBag) {
            error = "Invalid June character reservation";
            return false;
        }
        june14505::CharacterCatalog characters;
        june14505::SkillCatalog skills;
        if (!characters.Load(data + L"/14505/character-recipes.txt", error) ||
            !characters.LoadWeapons(data + L"/14505/weapon-loadouts.txt", error) ||
            !skills.Load(data + L"/14505/skill-catalog.txt", error))
            return false;
        const auto& saved = context.entry.character;
        const auto field = [&](const char* name, unsigned max, unsigned& value) {
            const auto at = saved.profileFields.find(name);
            return at != saved.profileFields.end() && ParseUnsigned(at->second, max, value);
        };
        unsigned race = 0, sex = 0, profession = 0;
        if (!field("race", 4, race) || !field("sex", 1, sex) || !field("profession", 8, profession)) {
            error = "Invalid June character skill loadout";
            return false;
        }
        const auto* recipe = characters.FindForCharacter(saved);
        if (!recipe) {
            error = "Missing June character skill loadout";
            return false;
        }
        std::unique_ptr<june14505::WorldSkills> actions(
            new june14505::WorldSkills(std::move(skills), recipe->loadout, id->character));
        if (!actions->LoadRecharges(saved, error))
            return false;
        return june14505::CreateInventoryActions(characters, saved, id->player, id->character, std::move(actions), out,
                                                 error);
    }
    bool CreateGameMessages(const BootstrapContext& context, std::shared_ptr<const GameMessages>& out,
                            std::string& error) const override {
        const auto* id = context.identity;
        if (context.catalog.build != 14505 || !id || !id->player || id->player == 255 ||
            id->character != 0x100u + id->player || id->firstBag) {
            error = "Invalid June character reservation";
            return false;
        }
        out = june14505::MakeGameMessages(id->character);
        error.clear();
        return true;
    }
};
} // namespace
const BuildProfile& June14505Profile() {
    static JuneProfile profile;
    return profile;
}
} // namespace glide
