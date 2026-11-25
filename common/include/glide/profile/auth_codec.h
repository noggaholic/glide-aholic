#pragma once
#include "glide/buffer.h"
#include "glide/character.h"
#include "glide/profile/character_codec.h"
#include <memory>

namespace glide {
enum class AuthAction {
    Unsupported,
    Login,
    SessionStatus,
    Control,
    Reply,
    SelectCharacter,
    DeleteCharacter,
    CharacterList,
    Assignment
};
struct AuthMessage {
    uint16_t id = 0;
    size_t consumed = 0;
    uint32_t handshake = 0, request = 0;
    uint32_t route = 0, map = 0, context = 0;
    ByteView identity;
    AuthAction action = AuthAction::Unsupported;
    std::string characterName;
};
class AuthCodec : public CharacterCodec {
  public:
    virtual ~AuthCodec() {}
    // Optional data-backed roster codec. The borrowed characters must outlive the result.
    virtual std::unique_ptr<AuthCodec> WithCharacters(const CharacterCodec&) const {
        return nullptr;
    }
    // Borrowed message fields remain valid while input storage is unchanged.
    virtual DecodeStatus Decode(ByteView input, bool authenticated, AuthMessage& out) const = 0;
    virtual bool Summary(const Character& character, Buffer& out) const = 0;
    virtual bool SessionStatus(uint64_t connection, Buffer& out) const = 0;
    virtual bool Reply(const AuthMessage& message, Buffer& out) const = 0;
    virtual bool Deletion(uint32_t, Buffer&) const {
        return false;
    }
    virtual bool Selection(const AccountState* account, uint32_t request, uint32_t secondsSince2000,
                           Buffer& out) const = 0;
    virtual bool CharacterList(const AccountState& account, uint32_t request, std::vector<Buffer>& frames) const = 0;
    virtual bool Assignment(const AuthMessage& message, uint32_t association, uint32_t map, uint32_t nonce,
                            Buffer& out) const = 0;
    virtual bool Login(const AccountState& account, uint32_t handshake, uint32_t request, uint32_t elapsed,
                       uint32_t secondsSince2000, std::vector<Buffer>& frames) const = 0;
};
} // namespace glide
