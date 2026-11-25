#include "game_request.h"

namespace glide {
namespace april14075 {
DecodeStatus DecodeGameRequest(ByteView input, GameRequest& out) {
    return june14505::DecodeGameRequestForBuild(input, 14075, out);
}
} // namespace april14075
} // namespace glide
