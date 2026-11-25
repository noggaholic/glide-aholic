#include "glide/profile/unloaded.h"
#include "profile.h"
#include "inventory_state.h"
#include "../build.h"
#include "auth_flow.h"
#include "character_catalog.h"
#include "create.h"
#include "bootstrap.h"
#include "game.h"
#include "game_wire.h"
#include "world_actions.h"
#include "game_initializer.h"
#include "world_maps.h"
#include "glide/files.h"

namespace glide {
namespace {
// July uses loaded codecs and per-connection AuthFlow. These legacy entry points
// must never substitute another build's messages if the required setup is absent.
class JulyProfile final : public BuildProfile {
  public:
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
    bool SupportsHealthStatePersistence() const override {
        return true;
    }
    bool SupportsConditionStatePersistence() const override {
        return true;
    }
    const BuildInfo& Info() const override {
        return kBuild14812;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::LocalTeleport, Capability::MapTransfer, Capability::TimeOfDay,
                                  Capability::Inspect, Capability::Announce, Capability::Markers});
    }
    bool GlobalPeerIds() const override {
        return true;
    }
    const StsVariant& Sts() const override {
        // Own cligate initializer passes SDK build1002 to its native STS manager.
        // Native password login over TLS; runtime capture remains pending.
        static const char* const optional[] = {"/Session/CreateSession"};
        static const StsVariant variant{true, "cligate.101.ncplatform.net", false, optional, 1,
                                        1002, StsLoginMode::TlsPassword};
        return variant;
    }
    const TransportVariant& Transport() const override {
        return july14812::Transport();
    }
    const GameInitializerCodec* GameInitializers() const override {
        return &july14812::Initializers();
    }
    bool SavedMapReturn(unsigned savedMap, unsigned requestedMap, uint32_t context, SpawnPoint& out) const override {
        if (context || requestedMap != 50 || (savedMap != 38 && savedMap != 94 && savedMap != 95 && savedMap != 96))
            return false;
        const auto maps = std::atomic_load(&maps_);
        const auto* geometry = maps ? maps->Find(50) : nullptr;
        if (!geometry)
            return false;
        out = {50, geometry->localArrival[0], geometry->localArrival[1], geometry->localArrival[2]};
        return true;
    }
    bool DefaultMapArrival(unsigned map, SpawnPoint& out) const override {
        const auto maps = std::atomic_load(&maps_);
        const auto* geometry = maps ? maps->Find(map) : nullptr;
        if (!geometry)
            return false;
        out = {map, geometry->localArrival[0], geometry->localArrival[1], geometry->localArrival[2]};
        return true;
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
        static july14812::GameWire unbound;
        return unbound;
    }
    const GameMessages& Game() const override {
        return july14812::ServiceMessages();
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        if (!july14812::LoadCharacterCodec(data, out, error))
            return false;
        const auto* characters = static_cast<const july14812::CharacterCatalog*>(out.get());
        std::shared_ptr<const july14812::WorldMapCatalog> maps(new july14812::WorldMapCatalog(characters->Maps()));
        std::atomic_store(&maps_, std::move(maps));
        return true;
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        return july14812::CreateAuthFlow(data, out, error);
    }
    bool LoadCreateFlow(const std::wstring& data, std::unique_ptr<CreateFlow>& out, std::string& error) const override {
        return july14812::CreateCharacterFlow(data, july14812::InitialSpawn(), out, error);
    }
    bool LoadBootstrap(const std::wstring& data, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return july14812::LoadBootstrap(data, out, error);
    }
    bool CreateGameWire(const std::string&, std::unique_ptr<WireMap>& out, std::string& error) const override {
        out.reset(new july14812::GameWire);
        error.clear();
        return true;
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (!Valid(context)) {
            error = "Invalid July character reservation";
            return false;
        }
        july14812::CharacterCatalog characters;
        if (!characters.Load(data + L"/14812/construction.bin", error) ||
            !characters.LoadSkills(data + L"/14812", error))
            return false;
        std::unique_ptr<WorldActions> result;
        if (!july14812::CreateInventoryActions(context.entry.character, characters, context.identity->itemBase,
                                               context.identity->character, result, error)) {
            error = "Invalid July world-action inventory";
            return false;
        }
        out = std::move(result);
        error.clear();
        return true;
    }
    bool CreateGameMessages(const BootstrapContext& context, std::shared_ptr<const GameMessages>& out,
                            std::string& error) const override {
        if (!Valid(context) || !context.serverPort) {
            error = "Invalid July character reservation or server port";
            return false;
        }
        auto result = july14812::MakeGameMessages(context.identity->character, context.serverPort);
        if (!result)
            return false;
        out = std::move(result);
        error.clear();
        return true;
    }

  private:
    mutable std::shared_ptr<const july14812::WorldMapCatalog> maps_;
    bool Valid(const BootstrapContext& context) const {
        const auto* id = context.identity;
        return context.catalog.build == 14812 && id && id->player && id->player < 255 &&
               id->character == 256u + id->player && !id->firstBag && id->itemBase && id->itemBase <= 0xffffffe0 &&
               !(id->itemBase & 31);
    }
};
} // namespace
const BuildProfile& July14812Profile() {
    static JulyProfile profile;
    return profile;
}
} // namespace glide
