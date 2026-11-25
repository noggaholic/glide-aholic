#pragma once
#include "auth_account.h"
#include "auth_summary.h"
#include "glide/kv.h"
#include <functional>
#include <string>
namespace glide {
namespace july14812 {
struct RosterEntry {
    uint32_t request = 0, retainedScalar = 0;
    std::array<uint8_t, 16> guid{};
    std::string name;
    CharacterSummary summary;
};
bool CharacterEntry(const RosterEntry&, uint32_t request, Buffer&);
bool DecodeCharacterEntry(ByteView, RosterEntry&);
bool Completion(uint32_t request, Buffer&);
bool ReadConfig(const KvDocument&, Buffer&);
class LoginReplies {
  public:
    bool Begin(const AccountSnapshot&, const std::vector<RosterEntry>&, ByteView config, std::vector<Buffer>&,
               const std::function<bool()>& admit = std::function<bool()>());
    bool ContentReported(std::vector<Buffer>&);
    bool Pending() const {
        return pending_;
    }

  private:
    bool pending_ = false, reported_ = false;
    Buffer completion_;
};
// July sends C12(total) then C13 chunks, including one empty chunk for total0.
// The final chunk and S7 are prepared before durable storage changes.
class ConfigUpload {
  public:
    bool Begin(uint32_t request, uint32_t total);
    bool Chunk(uint32_t request, ByteView, const std::function<bool(ByteView)>& save, Buffer& reply);
    bool Active() const {
        return active_;
    }

  private:
    bool active_ = false;
    uint32_t request_ = 0, total_ = 0;
    Buffer bytes_;
};
} // namespace july14812
} // namespace glide
