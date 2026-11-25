#pragma once
#include "auth_data.h"
#include <string>
#include <functional>
#include "auth_request.h"
#include "glide/kv.h"
namespace glide {
namespace hot49284 {
struct RosterEntry {
    std::array<uint8_t, 16> guid{};
    uint32_t retainedScalar = 0;
    std::array<uint64_t, 2> retainedValues{};
    std::string name;
    CharacterSummary summary;
};
// Call only after authentication and login completion. Prepare S7 before saving;
// failed validation or durable storage leaves the caller's reply untouched.
bool CommitConfig(const AuthRequest&, const std::function<bool(ByteView)>& save, Buffer& reply);
bool ReadConfig(const KvDocument&, Buffer& out);
// Validate own C29 before issuing any permit. The world GUID is mission context,
// not account/character identity. This helper never authenticates the caller.
bool ValidateRedirect(const AuthRequest&, uint32_t previousRequest);
// association/map must come from an authorized server referral, never C29.
bool RedirectReply(const AuthRequest&, uint32_t association, uint32_t map, uint16_t port, Buffer& out);
bool CharacterEntry(const RosterEntry&, uint32_t request, Buffer&);
// The snapshot is fully encoded before publishing Account8. Completion waits
// for the first content report; no later caller mutations can change it.
class LoginReplies {
  public:
    bool Begin(const AccountSnapshot&, const std::vector<RosterEntry>&, ByteView config, std::vector<Buffer>& out);
    bool ContentReported(std::vector<Buffer>& out);
    bool Pending() const {
        return pending_;
    }

  private:
    bool pending_ = false, contentReported_ = false;
    Buffer completion_;
};
} // namespace hot49284
} // namespace glide
