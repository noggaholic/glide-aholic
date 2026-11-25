#include "../build.h"
#include "auth.h"
#include "auth_flow.h"
#include "equipment_catalog.h"
#include "inventory_state.h"
#include "bootstrap.h"
#include "create.h"
#include "game.h"
#include "protocol.h"
#include "glide/profile/build_profile.h"
#include <Windows.h>
#include <ctime>

namespace glide {
namespace {
class ChinaProfile final : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild167409;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::LocalTeleport, Capability::MapTransfer, Capability::Mounts,
                                  Capability::TimeOfDay, Capability::Inspect, Capability::Announce, Capability::Markers,
                                  Capability::Glider, Capability::FacetActions, Capability::CombatTraitSelection});
    }
    bool SupportsInventoryPersistence() const override {
        return true;
    }
    bool SupportsProfessionStatePersistence() const override {
        return true;
    }
    bool SupportsLegendStatePersistence() const override {
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
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (context.catalog.build != 167409 || !context.identity) {
            error = "Invalid China inventory context";
            return false;
        }
        china167409::EquipmentCatalog catalog;
        return catalog.Load(data, error) &&
               china167409::CreateInventoryActions(catalog, context.entry.character, *context.identity, out, error);
    }
    const AuthCodec& Auth() const override {
        return China167409Auth();
    }
    const CreateFlow& Create() const override {
        return China167409Create();
    }
    const GameBootstrap& Bootstrap() const override {
        return China167409Bootstrap();
    }
    bool LoadBootstrap(const std::wstring& root, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return LoadChina167409Bootstrap(root, out, error);
    }
    const WireMap& Wire() const override {
        return China167409Wire();
    }
    const GameMessages& Game() const override {
        return China167409Game();
    }
    const TransportVariant& Transport() const override {
        static const TransportVariant transport{111, true};
        return transport;
    }
    const StsVariant& Sts() const override {
        static const char* const empty[] = {
            "/Auth2f/Check", "/Auth/GetUserInfo", "/Game.gw2.PvpQuery.5/ListArenas", "/Game.gw2.PvpStat.5/Query",
            "/Game.com.Wallet/SetWatch", "/Game.gw2.TrnSrch.5/Search", "/Game.gw2.GemStore/GetLimits",
            "/Game.gw2.GemStore/GetHighlights", "/Game.gw2.ItemSearch/GemStoreSearch",
            // Original China world entry uses the regional service names. Send an authenticated empty-service reply.
            "/Game.gw2cn.PvpQuery.5/ListArenas", "/Game.gw2cn.PvpQuery.5/ListGameModes", "/Game.gw2cn.PvpStat.5/Query",
            "/Ladder/SetWatch", "/Game.gw2cn.TrnSrch.5/Search", "/Game.gw2cn.GemStore/GetLimits",
            "/Game.gw2cn.GemStore/GetHighlights", "/Game.gw2cn.ItemSearch/GemStoreSearch"};
        static const StsVariant sts{true,  "cligate.101.ncplatform.net",     true,
                                    empty, sizeof(empty) / sizeof(empty[0]), 1004};
        return sts;
    }
    bool LoadCharacterCodec(const std::wstring& data, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        std::unique_ptr<china167409::EquipmentCatalog> catalog(new china167409::EquipmentCatalog);
        if (!catalog->Load(data, error))
            return false;
        out = std::move(catalog);
        error.clear();
        return true;
    }
    bool CreateAuthFlow(const std::wstring& data, std::unique_ptr<AuthFlow>& out, std::string& error) const override {
        std::unique_ptr<CharacterCodec> characters;
        if (!LoadCharacterCodec(data, characters, error))
            return false;
        auto flow = china167409::CreateAuthFlow(
            [] {
                const auto now = std::time(nullptr);
                return now < 0 ? uint64_t(0) : static_cast<uint64_t>(now);
            },
            [] { return static_cast<uint32_t>(GetTickCount()); }, std::move(characters));
        if (!flow) {
            error = "China Auth flow could not be created";
            return false;
        }
        out = std::move(flow);
        error.clear();
        return true;
    }
};
} // namespace
const BuildProfile& China167409Profile() {
    static const ChinaProfile profile;
    return profile;
}
} // namespace glide
