#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
namespace glide {
namespace hot49284 {
enum class AuthOperation {
    Clock,
    Machine,
    Identity,
    Login,
    Content,
    Logout,
    Config,
    Agreement,
    Select,
    Roster,
    Redirect,
    KeepAlive,
    SystemReport,
    ViewLeave,
    CurrentWorld,
    Delete
};
struct AuthRequest {
    AuthOperation operation = AuthOperation::Clock;
    uint16_t id = 0;
    size_t consumed = 0;
    uint32_t request = 0;
    std::array<uint32_t, 5> routing{};
    uint8_t agreement = 0;
    uint32_t world = 0;
    uint8_t temporary = 0;
    ByteView identity, data;
    std::string name;
};
// Views borrow input. Incomplete/unknown/malformed requests leave output untouched.
DecodeStatus DecodeAuthRequest(ByteView, AuthRequest&);
} // namespace hot49284
} // namespace glide
