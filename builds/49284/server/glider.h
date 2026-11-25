#pragma once
#include "glide/profile/game_messages.h"
namespace glide {
namespace hot49284 {
bool GliderCommand(uint16_t agent, uint32_t sequence, bool active, Buffer& out);
// One controller per authorized world session. Caller supplies monotonic milliseconds.
class GliderController {
  public:
    RelayResult Update(ByteView motion, uint16_t ownedAgent, uint32_t sequence, uint64_t now, Buffer& out);
    bool Active() const {
        return active_;
    }

  private:
    bool active_ = false, reported_ = false, descending_ = false, inputSeen_ = false;
    uint8_t state_ = 0;
    float z_ = 0;
    uint64_t reportAt_ = 0;
    uint32_t inputToken_ = 0;
};
} // namespace hot49284
} // namespace glide
