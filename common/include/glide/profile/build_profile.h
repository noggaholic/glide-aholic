#pragma once
#include "glide/builds.h"
#include "glide/capabilities.h"
#include "glide/profile/auth_codec.h"
#include "glide/profile/auth_flow.h"
#include "glide/profile/game_initializer.h"
#include <memory>
#include "glide/profile/create_flow.h"
#include "glide/profile/game_bootstrap.h"
#include "glide/profile/wire_map.h"
#include "glide/profile/game_messages.h"
#include "glide/profile/world_actions.h"

namespace glide {
enum class StsLoginMode { Sso, TlsPassword };
struct StsVariant {
    bool tlsBeforeLogin;
    const char* hostname;
    bool passwordBase64 = false;               // Encoding of the Password field on the wire.
    const char* const* emptyReplies = nullptr; // Explicit optional queries supported with an empty result.
    size_t emptyReplyCount = 0;
    uint32_t connectionBuild = 0; // Native STS version; may differ from the game executable build.
    StsLoginMode loginMode = StsLoginMode::Sso;
    const char* gameAccountAlias = nullptr; // Null keeps the display-name alias used by other clients.
};
struct TransportVariant {
    uint32_t program;
    bool serverFrames;
    KeyExchangeRecord keyRecord = KeyExchangeRecord::LegacyFixed66;
};
// Interfaces are added as their protocol components are transferred and tested.
class BuildProfile {
  public:
    virtual ~BuildProfile() {}
    virtual const BuildInfo& Info() const = 0;
    virtual CapabilitySet Capabilities() const {
        return {};
    }
    virtual bool GlobalPeerIds() const {
        return false;
    }
    virtual bool SupportsInventoryPersistence() const {
        return false;
    }
    virtual bool SupportsSkillRechargePersistence() const {
        return SupportsInventoryPersistence();
    }
    virtual bool SupportsSkillStatePersistence() const {
        return false;
    }
    virtual bool SupportsProfessionStatePersistence() const {
        return false;
    }
    virtual bool SupportsLegendStatePersistence() const {
        return false;
    }
    virtual bool SupportsHealthStatePersistence() const {
        return false;
    }
    virtual bool SupportsConditionStatePersistence() const {
        return false;
    }
    virtual const GameInitializerCodec* GameInitializers() const {
        return nullptr;
    }
    // A saved instance may require a specific native return destination on login.
    // Default profiles keep the saved map; this never grants arbitrary Auth travel.
    virtual bool SavedMapReturn(unsigned, unsigned, uint32_t, SpawnPoint&) const {
        return false;
    }
    // Optional local arrival for map-only travel when no own waypoint is available.
    virtual bool DefaultMapArrival(unsigned, SpawnPoint&) const {
        return false;
    }
    virtual const StsVariant& Sts() const = 0;
    virtual const AuthCodec& Auth() const = 0;
    virtual bool LoadCharacterCodec(const std::wstring&, std::unique_ptr<CharacterCodec>& out,
                                    std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
    // Null retains the existing stateless codec; stateful profiles own their connection flow.
    virtual bool CreateAuthFlow(const std::wstring&, std::unique_ptr<AuthFlow>& out, std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
    virtual const CreateFlow& Create() const = 0;
    // Optional data-backed creator, owned by the server for its entire bound lifetime.
    virtual bool LoadCreateFlow(const std::wstring&, std::unique_ptr<CreateFlow>& out, std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
    virtual const GameBootstrap& Bootstrap() const = 0;
    virtual bool LoadBootstrap(const std::wstring&, std::unique_ptr<GameBootstrap>& out, std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
    virtual const WireMap& Wire() const = 0;
    // Owned by the world worker through both loading and gameplay. Null uses Wire().
    virtual bool CreateGameWire(const std::string&, std::unique_ptr<WireMap>& out, std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
    virtual const GameMessages& Game() const = 0;
    // Uses the same entry as Bootstrap::Steps. Stateful profiles return an owned object.
    virtual bool CreateGameMessages(const BootstrapContext&, std::shared_ptr<const GameMessages>& out,
                                    std::string& error) const {
        out = std::shared_ptr<const GameMessages>(std::shared_ptr<const GameMessages>(), &Game());
        error.clear();
        return true;
    }
    virtual const TransportVariant& Transport() const = 0;
    virtual bool CreateWorldActions(const BootstrapContext&, const std::wstring&, std::unique_ptr<WorldActions>& out,
                                    std::string& error) const {
        out.reset();
        error.clear();
        return true;
    }
};
namespace BuildRegistry {
const BuildProfile* Find(uint32_t build);
}
} // namespace glide
