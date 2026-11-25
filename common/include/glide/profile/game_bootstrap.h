#pragma once
#include "glide/buffer.h"
#include "glide/character.h"
#include "glide/catalog.h"
#include <vector>

namespace glide {
struct WorldEntry {
    AccountState account;
    Character character;
    uint32_t association = 0, joinContext = 0;
    uint8_t joinPrefix[4] = {};
    bool transfer = false;
    unsigned previousMap = 0;
};
enum class GameConfirmation { Hello, TransferHello, MapContextReady, MapLoadAccepted, MapLoaderReady, WorldBuilt };
struct BootstrapStep {
    enum class Kind { Send, WaitFor };
    Kind kind = Kind::Send;
    const char* name = "";
    Buffer payload;
    GameConfirmation confirmation = GameConfirmation::Hello;
    uint32_t timeoutMs = 0;
};
struct PeerIdentity;
struct BootstrapContext {
    const WorldEntry& entry;
    const Catalog& catalog;
    uint32_t sequence = 1, timeTicks = 0;
    uint16_t serverPort = 0;
    float timeRate = 1;
    bool timePaused = false;
    // Reserved before loading; profiles may also use it for the owned actor.
    const PeerIdentity* identity = nullptr;
};
struct PeerIdentity {
    uint8_t player = 0;
    uint16_t character = 0;
    uint32_t firstBag = 0;
    uint32_t itemBase = 0; // Session-specific object range, independent of reusable player slots.
};
class GameBootstrap {
  public:
    virtual ~GameBootstrap() {}
    // Produces an ordered recipe; failure leaves the caller's recipe unchanged.
    virtual bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const = 0;
    virtual bool PeerSteps(const BootstrapContext& context, const PeerIdentity& identity,
                           std::vector<BootstrapStep>& out) const = 0;
    virtual bool PeerSlot(unsigned slot, PeerIdentity& out) const = 0;
    // Called once with the unique world connection owner, before publishing identity.
    virtual bool BindSession(uint64_t, PeerIdentity&) const {
        return true;
    }
    virtual bool PeerRemove(const PeerIdentity& identity, uint32_t sequence, Buffer& out) const = 0;
};
} // namespace glide
