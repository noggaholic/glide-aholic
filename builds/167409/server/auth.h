#pragma once
#include "glide/profile/auth_codec.h"
namespace glide {
const AuthCodec& China167409Auth();
namespace china167409 {
bool EncodeAccount(const AccountState& account, uint32_t request, uint64_t unixSeconds, Buffer& out);
bool EncodeRoster(const AccountState& account, uint32_t request, Buffer& out);
} // namespace china167409
} // namespace glide
