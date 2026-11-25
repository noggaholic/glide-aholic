#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
namespace glide {
namespace july14812 {
enum class AuthOperation {
    Clock,
    Machine,
    Identity,
    Login,
    Content,
    Reauthenticate,
    Sequence,
    Select,
    Delete,
    ConfigBegin,
    ConfigChunk,
    Redirect,
    Agreement,
    CurrentWorld,
    Roster,
    Unclassified
};
struct AuthRequest {
    AuthOperation operation = AuthOperation::Unclassified;
    uint16_t id = 0;
    size_t consumed = 0;
    uint32_t request = 0, totalBytes = 0, world = 0;
    std::array<uint32_t, 4> routing{};
    uint8_t agreement = 0;
    ByteView identity, data;
    std::string name;
};
// One bounded native record. Unclassified means known schema, not an authorized operation.
// Views borrow input; output changes only on Complete.
DecodeStatus DecodeAuthRequest(ByteView, AuthRequest&);
} // namespace july14812
} // namespace glide
