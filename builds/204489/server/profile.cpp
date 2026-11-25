#include "../build.h"
#include "equipment_catalog.h"
#include "inventory_state.h"
#include "glide/profile/build_profile.h"

namespace glide {
const AuthCodec& Build204489Auth();
const CreateFlow& Build204489Create();
const GameBootstrap& Build204489Bootstrap();
const WireMap& Build204489Wire();
const GameMessages& Build204489Game();
namespace {
class ServerProfile204489 : public BuildProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild204489;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::LocalTeleport, Capability::MapTransfer, Capability::Mounts,
                                  Capability::TimeOfDay, Capability::Inspect, Capability::Announce, Capability::Markers,
                                  Capability::Glider, Capability::FacetActions, Capability::CombatTraitSelection});
    }
    bool SupportsSkillRechargePersistence() const override {
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
    bool SupportsInventoryPersistence() const override {
        return true;
    }
    bool SupportsProfessionStatePersistence() const override {
        return true;
    }
    bool SupportsLegendStatePersistence() const override {
        return true;
    }
    bool CreateWorldActions(const BootstrapContext& context, const std::wstring& data,
                            std::unique_ptr<WorldActions>& out, std::string& error) const override {
        if (context.catalog.build != 204489 || !context.identity) {
            error = "Invalid Build 204489 weapon action context";
            return false;
        }
        build204489::EquipmentCatalog catalog;
        return catalog.Load(data, error) &&
               build204489::CreateInventoryActions(catalog, context.entry.character, *context.identity, out, error);
    }
    const AuthCodec& Auth() const override {
        return Build204489Auth();
    }
    const CreateFlow& Create() const override {
        return Build204489Create();
    }
    const GameBootstrap& Bootstrap() const override {
        return Build204489Bootstrap();
    }
    bool LoadBootstrap(const std::wstring& root, std::unique_ptr<GameBootstrap>& out,
                       std::string& error) const override {
        return LoadBuild204489Bootstrap(root, out, error);
    }
    bool LoadCharacterCodec(const std::wstring& root, std::unique_ptr<CharacterCodec>& out,
                            std::string& error) const override {
        return LoadBuild204489CharacterCodec(root, out, error);
    }
    const WireMap& Wire() const override {
        return Build204489Wire();
    }
    const GameMessages& Game() const override {
        return Build204489Game();
    }
    const TransportVariant& Transport() const override {
        static const TransportVariant transport{101, true};
        return transport;
    }
    const StsVariant& Sts() const override {
        // Observed native queries; retain the old local server's empty-result behavior.
        static const char* const emptyReplies[] = {"/Auth2f/Check",
                                                   "/Auth/GetUserInfo",
                                                   "/Game.gw2.PvpQuery.5/ListArenas",
                                                   "/Game.gw2.PvpStat.5/Query",
                                                   "/Game.com.Wallet/SetWatch",
                                                   "/Game.gw2.TrnSrch.5/Search",
                                                   "/Game.gw2.GemStore/GetLimits",
                                                   "/Game.gw2.GemStore/GetHighlights",
                                                   "/Game.gw2.ItemSearch/GemStoreSearch"};
        static const StsVariant variant{
            true, "cligate.101.ncplatform.net", true, emptyReplies, sizeof(emptyReplies) / sizeof(emptyReplies[0]),
            1004};
        return variant;
    }
};
} // namespace
const BuildProfile& Build204489Profile() {
    static ServerProfile204489 profile;
    return profile;
}
} // namespace glide
