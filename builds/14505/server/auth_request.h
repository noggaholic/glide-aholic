#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
namespace june14505 {
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
    LegacyControl
};
struct AuthRequest {
    AuthOperation operation = AuthOperation::Clock;
    uint16_t id = 0;
    size_t consumed = 0;
    uint32_t request = 0, totalBytes = 0, routing[4] = {};
    uint8_t agreement = 0;
    ByteView identity, data;
    std::string name;
};
// One message only. Byte views borrow input; output changes only on Complete.
// Operation decoding does not authenticate a connection or grant game access.
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out);
DecodeStatus DecodeAuthRequestForBuild(ByteView input, unsigned build, AuthRequest& out);
bool SessionReady(uint32_t connection, Buffer& out);
} // namespace june14505
} // namespace glide
