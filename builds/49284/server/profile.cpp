#include "glide/profile/unloaded.h"
#include "starter_skill_data.h"
#include "glide/item_bindings.h"
#include "profile.h"
#include "../build.h"
#include "auth_flow.h"
#include "character_catalog.h"
#include "create.h"
#include "bootstrap.h"
#include "game.h"
#include "game_wire.h"
#include "world_actions.h"
#include "inventory_state.h"
#include "game_initializer.h"
#include "glide/files.h"

namespace glide {
namespace {
// HoT uses loaded codecs and per-connection AuthFlow. These legacy entry points
// must never substitute another build's messages if the required setup is absent.
class HotProfile final : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild49284;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::LocalTeleport, Capability::Glider, Capability::TimeOfDay,
                                  Capability::Inspect, Capability::Announce, Capability::Markers,
                                  Capability::MapTransfer});
    }
    bool GlobalPeerIds() const override {
        return true;
    }
    bool SupportsInventoryPersistence() const override {
        return true;
    }
    bool SupportsSkillRechargePersistence() const override {
        return true;
    }
    bool SupportsSkillStatePersistence() const override {
        return true;
    }
    bool SupportsProfessionStatePersistence() const override {
        return true;
    }
    bool SupportsLegendStatePersistence() const override {
        return true;
    }
    bool SupportsHealthStatePersistence() const override {
        return true;
    }
    bool SupportsConditionStatePersistence() const override {
        return true;
    }
    const StsVariant& Sts() const override {
        // Native SSO over TLS; wire password is Base64. STS alias1004 confirmed by native startup capture.
        static const StsVariant variant{
            true, "cligate.101.ncplatform.net", true, nullptr, 0, 1004, StsLoginMode::Sso, "Guild Wars 2"};
        return variant;
    }
    const TransportVariant& Transport() const override {
        static const TransportVariant variant{101, true, KeyExchangeRecord::SizedAligned};
        return variant;
    }
    const GameInitializerCodec* GameInitializers() const override {
        return &hot49284::Initializers();
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
        static hot49284::GameWire unbound;
        return unbound;
    }
    const GameMessages& Game() const override {
        return hot49284::ServiceMessages();
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        return LoadItemBindings(data + L"/49284/server/item-bindings.bin", 49284, error) &&
               hot49284::LoadStarterSkills(data + L"/49284/server/starter-skills.bin",
                                           data + L"/49284/server/starting-loadouts.bin", error) &&
               hot49284::LoadCharacterCodec(data, out, error);
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        return hot49284::CreateAuthFlow(data, out, error);
    }
    bool LoadCreateFlow(const std::wstring& data, std::unique_ptr<CreateFlow>& out, std::string& error) const override {
        return hot49284::CreateCharacterFlow(data, {15, -15525.0f / 32, 21974.0f / 32, 2506.0f / 32}, out, error);
    }
    bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return hot49284::LoadBootstrap(data, out, error);
    }
    bool CreateGameWire(const std::string&, std::unique_ptr<WireMap>& out, std::string& error) const override {
        out.reset(new hot49284::GameWire);
        error.clear();
        return true;
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (!Valid(context)) {
            error = "Invalid HoT character reservation";
            return false;
        }
        hot49284::CharacterCatalog catalog;
        return catalog.Load(data + L"/49284/construction.bin", error) &&
               hot49284::CreateInventoryActions(context.entry.character, catalog, context.identity->itemBase,
                                                context.identity->character, out, error);
    }
    bool CreateGameMessages(const BootstrapContext& context, std::shared_ptr<const GameMessages>& out,
                            std::string& error) const override {
        if (!Valid(context) || !context.serverPort) {
            error = "Invalid HoT character reservation or server port";
            return false;
        }
        auto result = hot49284::MakeGameMessages(context.identity->character, context.serverPort);
        if (!result)
            return false;
        out = std::move(result);
        error.clear();
        return true;
    }

  private:
    bool Valid(const BootstrapContext& context) const {
        const auto* id = context.identity;
        return context.catalog.build == 49284 && id && id->player && id->player < 255 &&
               id->character == 256u + id->player && !id->firstBag && id->itemBase && id->itemBase <= 0xffffffe0 &&
               !(id->itemBase & 31);
    }
};
} // namespace
const BuildProfile& Hot49284Profile() {
    static HotProfile profile;
    return profile;
}
} // namespace glide
