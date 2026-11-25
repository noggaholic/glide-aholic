#pragma once
#include "auth_codec.h"
#include "create_flow.h"
#include "game_bootstrap.h"

namespace glide {
// Reject calls until the build-specific data-backed implementations are loaded.
class UnloadedAuth final : public AuthCodec {
  public:
    DecodeStatus Decode(ByteView, bool, AuthMessage&) const override {
        return DecodeStatus::Malformed;
    }
    bool Summary(const Character&, Buffer&) const override {
        return false;
    }
    bool SessionStatus(uint64_t, Buffer&) const override {
        return false;
    }
    bool Reply(const AuthMessage&, Buffer&) const override {
        return false;
    }
    bool Selection(const AccountState*, uint32_t, uint32_t, Buffer&) const override {
        return false;
    }
    bool CharacterList(const AccountState&, uint32_t, std::vector<Buffer>&) const override {
        return false;
    }
    bool Assignment(const AuthMessage&, uint32_t, uint32_t, uint32_t, Buffer&) const override {
        return false;
    }
    bool Login(const AccountState&, uint32_t, uint32_t, uint32_t, uint32_t, std::vector<Buffer>&) const override {
        return false;
    }
};
class UnloadedCreate final : public CreateFlow {
  public:
    DecodeStatus Decode(ByteView, CreateMessage&) const override {
        return DecodeStatus::Malformed;
    }
    bool ValidateSubmit(const Character&) const override {
        return false;
    }
    const SpawnPoint& InitialLocation() const override {
        static const SpawnPoint empty{};
        return empty;
    }
    bool Ready(Buffer&) const override {
        return false;
    }
    bool Signal(Buffer&) const override {
        return false;
    }
    bool Created(const Guid&, const Character&, Buffer&) const override {
        return false;
    }
};
class UnloadedBootstrap final : public GameBootstrap {
  public:
    bool Steps(const BootstrapContext&, std::vector<BootstrapStep>&) const override {
        return false;
    }
    bool PeerSteps(const BootstrapContext&, const PeerIdentity&, std::vector<BootstrapStep>&) const override {
        return false;
    }
    bool PeerSlot(unsigned, PeerIdentity&) const override {
        return false;
    }
    bool PeerRemove(const PeerIdentity&, uint32_t, Buffer&) const override {
        return false;
    }
};
} // namespace glide
