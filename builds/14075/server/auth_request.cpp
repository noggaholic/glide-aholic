#include "auth_request.h"

namespace glide {
namespace april14075 {
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out) {
    return june14505::DecodeAuthRequestForBuild(input, 14075, out);
}
bool SessionReady(uint32_t connection, Buffer& out) {
    Buffer result;
    if (!result.U16(1) || !result.U32(connection))
        return false;
    out = std::move(result);
    return true;
}
} // namespace april14075
} // namespace glide
