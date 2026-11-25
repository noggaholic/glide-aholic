#pragma once
#include "../../14505/server/auth_request.h"

namespace glide {
namespace april14075 {
using AuthRequest = june14505::AuthRequest;
using AuthOperation = june14505::AuthOperation;
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out);
bool SessionReady(uint32_t connection, Buffer& out);
} // namespace april14075
} // namespace glide
