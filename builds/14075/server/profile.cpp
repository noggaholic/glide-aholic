#include "profile.h"
#include "glide/item_bindings.h"
#include "../build.h"
#include "auth_flow.h"
#include "character_catalog.h"
#include "create.h"
#include "bootstrap.h"
#include "game.h"
#include "game_wire.h"
#include "inventory_state.h"
#include "../../14505/server/profile.h"
#include <algorithm>

namespace glide {
namespace {
class AprilProfile final : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14075;
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
    bool SupportsProfessionStatePersistence() const override {
        return true;
    }
    bool SupportsHealthStatePersistence() const override {
        return true;
    }
    bool SupportsConditionStatePersistence() const override {
        return true;
    }
    // The retained server uses the same legacy STS and MsgConn transport for April.
    const StsVariant& Sts() const override {
        return June14505Profile().Sts();
    }
    const TransportVariant& Transport() const override {
        return June14505Profile().Transport();
    }
    // Only the shared unavailable implementations are reused. The factories below
    // always load April data and never substitute June's functional codecs.
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
        static april14075::GameWire unbound("");
        return unbound;
    }
    const GameMessages& Game() const override {
        return april14075::ServiceMessages();
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        return LoadItemBindings(data + L"/14075/server/item-bindings.bin", 14075, error) &&
               april14075::LoadCharacterCodec(data, out, error);
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        return april14075::CreateAuthFlow(data, out, error);
    }
    bool LoadCreateFlow(const std::wstring& data, std::unique_ptr<CreateFlow>& out, std::string& error) const override {
        // Map 50 land candidate from June 7, tested in the original April client.
        // This is a QA start position, not a recovered April waypoint identity.
        return april14075::CreateCharacterFlow(data, {50, 117.836f, 198.896f, 37.328f}, out, error);
    }
    bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return april14075::LoadBootstrap(data, out, error);
    }
    bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) const override {
        return april14075::CreateGameWire(login, out, error);
    }
    bool CreateGameMessages(const BootstrapContext& context, std::shared_ptr<const GameMessages>& out,
                            std::string& error) const override {
        if (!Valid(context) || !context.serverPort) {
            error = "Invalid April character reservation or server port";
            return false;
        }
        auto candidate = april14075::MakeGameMessages(context.identity->character);
        if (!candidate) {
            error = "Invalid April character agent";
            return false;
        }
        out = std::move(candidate);
        error.clear();
        return true;
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (!Valid(context)) {
            error = "Invalid April character reservation";
            return false;
        }
        april14075::CharacterCatalog characters;
        june14505::SkillCatalog skills(14075);
        if (!characters.LoadData(data, error) || !skills.Load(data + L"/14075/skill-catalog.txt", error))
            return false;
        const auto& saved = context.entry.character;
        const auto* recipe = characters.FindForCharacter(saved);
        if (!recipe) {
            error = "Missing April character skill loadout";
            return false;
        }
        std::unique_ptr<june14505::WorldSkills> actions(
            new june14505::WorldSkills(std::move(skills), recipe->loadout, context.identity->character));
        if (!actions->LoadRecharges(saved, error))
            return false;
        return april14075::CreateInventoryActions(characters, saved, context.identity->player,
                                                  context.identity->character, out, error, std::move(actions));
    }

  private:
    bool Valid(const BootstrapContext& context) const {
        const auto* id = context.identity;
        return context.catalog.build == 14075 && id && id->player && id->player < 255 &&
               id->character == 256u + id->player && !id->firstBag && !id->itemBase &&
               std::any_of(
                   context.entry.character.guid.bytes, context.entry.character.guid.bytes + 16,
                   [](uint8_t byte) { return byte != 0; });
    }
};
} // namespace
const BuildProfile& April14075Profile() {
    static AprilProfile profile;
    return profile;
}
} // namespace glide
