#pragma once
#include "glide/character.h"
#include <array>
#include <functional>

namespace glide {
namespace june14505 {
// Armor values are ItemDef DataIds; dye values are resolved palette indices.
// Neither armor UIDs nor ColorDef DataIds are sent in these fields.
struct StartingWeapon {
    uint8_t slot = 0;
    uint32_t item = 0;
};
struct RosterCharacter {
    std::vector<StartingWeapon> weapons;
    std::vector<uint32_t> learnedSkills;
    Guid guid;
    std::string name;
    uint8_t level = 1, profession = 1;
    uint32_t map = 0;
    std::array<uint8_t, 45> appearance{};
    std::array<uint16_t, 6> armor{};
    std::array<std::array<uint16_t, 4>, 6> dyes{};
};
bool CharacterSummary(const RosterCharacter& character, Buffer& out);
bool CharacterEntry(const RosterCharacter& character, uint32_t request, Buffer& out);
bool AccountReply(const AccountState& account, const Guid& selected, uint32_t request, ByteView contentSeed,
                  Buffer& out);
bool LoginComplete(const std::vector<RosterCharacter>& characters, uint32_t request, std::vector<Buffer>& out,
                   ByteView config = ByteView());
bool ReadConfig(const KvDocument& document, Buffer& out);
bool SelectionReply(uint32_t request, uint32_t map, Buffer& out);
// A connection owns one upload. Finish publishes success only after saving succeeds.
class ConfigUpload {
  public:
    bool Begin(uint32_t request, uint32_t total);
    bool Chunk(uint32_t request, ByteView bytes);
    bool Finish(const std::function<bool(ByteView)>& save, Buffer& out);
    bool Active() const {
        return active_;
    }
    bool Ready() const {
        return active_ && receivedChunk_ && bytes_.Size() == total_;
    }

  private:
    uint32_t request_ = 0, total_ = 0;
    bool active_ = false, receivedChunk_ = false;
    Buffer bytes_;
};

// Per authenticated Auth connection. The caller must claim its STS ticket first.
// Preflight both phases before publishing account data. A content report is retained
// for later reauthentication on this connection, matching the existing June flow.
class LoginReplies {
  public:
    bool Begin(const AccountState& account, const std::vector<RosterCharacter>& characters, uint32_t request,
               ByteView contentSeed, std::vector<Buffer>& out, ByteView config = ByteView());
    bool BeginPrepared(Buffer state, std::vector<Buffer> completion, std::vector<Buffer>& out);
    bool ContentReported(std::vector<Buffer>& out);
    bool Pending() const {
        return pending_;
    }

  private:
    bool contentReported_ = false, pending_ = false;
    std::vector<Buffer> completion_;
};
} // namespace june14505
} // namespace glide
