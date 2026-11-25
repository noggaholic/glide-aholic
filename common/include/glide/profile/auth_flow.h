#pragma once
#include "glide/character.h"

namespace glide {
// Connection-bound account operations; profile code cannot choose another owner/session.
class AuthServices {
  public:
    virtual ~AuthServices() {}
    virtual uint64_t ConnectionId() const = 0;
    virtual bool Active() = 0;
    // Withdraw only unclaimed referrals issued by this Auth connection.
    virtual bool RevokePending() {
        return false;
    }
    virtual uint16_t Port() const {
        return 0;
    }
    // Bound referral: route, requested map, context, slots -> issued association/map.
    // A failed operation leaves both output values untouched.
    virtual bool Refer(uint32_t, uint32_t, uint32_t, unsigned, uint32_t&, uint32_t&, std::string&) {
        return false;
    }
    virtual bool Claim(ByteView identity) = 0;
    // July preflights account/content before consuming its issued game token.
    // Unsupported services fail closed; preparation never claims a ticket.
    virtual bool PrepareLogin(ByteView, AccountState&, std::string&) {
        return false;
    }
    virtual bool ClaimPrepared(ByteView) {
        return false;
    }
    virtual bool Load(AccountState& out, std::string& error) = 0;
    virtual bool Select(const std::string& name, AccountState& out, std::string& error) = 0;
    virtual bool Delete(const std::string& name, std::string& error) = 0;
    virtual bool SaveConfig(ByteView bytes, std::string& error) = 0;
    virtual bool Agreement(uint8_t version, std::string& error) = 0;
    virtual bool Assign(uint32_t route, uint32_t& association, uint32_t& map, uint32_t& nonce, std::string& error) = 0;
};
class AuthFlow {
  public:
    virtual ~AuthFlow() {}
    // Incomplete input has no side effects. Errors close the connection; only Complete
    // publishes consumed bytes and complete response frames.
    virtual DecodeStatus Step(ByteView input, AuthServices& services, size_t& consumed, std::vector<Buffer>& frames,
                              std::string& error) = 0;
};
} // namespace glide
