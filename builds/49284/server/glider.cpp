#include "glider.h"
#include "motion.h"
#include "glider_protocol.h"
#include <utility>
namespace glide {
namespace hot49284 {
bool GliderCommand(uint16_t agent, uint32_t sequence, bool active, Buffer& out) {
    uint8_t bytes[34];
    size_t written = 0;
    if (!glide_49284_glider_encode(agent, sequence, active ? 1 : 0, active ? 256.0f : 0.0f, bytes, sizeof(bytes),
                                   &written))
        return false;
    Buffer result;
    if (!result.Append(ByteView(bytes, written)))
        return false;
    out = std::move(result);
    return true;
}
RelayResult GliderController::Update(ByteView input, uint16_t ownedAgent, uint32_t sequence, uint64_t now,
                                     Buffer& out) {
    MotionMessage message{};
    size_t consumed = 0;
    const auto status = DecodeMotion(input, message, consumed);
    if (status == DecodeStatus::Unknown)
        return RelayResult::Ignored;
    if (status != DecodeStatus::Complete || consumed != input.size || !ownedAgent ||
        (message.id != 15 && message.id != 16 && message.agent != ownedAgent))
        return RelayResult::Invalid;
    auto next = *this;
    if (message.id == 16) {
        next.descending_ =
            reported_ && (state_ & ~1u) == 2 && (message.state & ~1u) == 2 && message.position[2] < z_ - 0.001f;
        next.state_ = message.state;
        next.z_ = message.position[2];
        next.reported_ = true;
        next.reportAt_ = now;
        if ((message.state & ~1u) != 2)
            next.active_ = false;
    } else if (message.id == 18 && message.state == 3 && message.input_size == 1 && message.input[0] == 0x0e) {
        if (!inputSeen_ || message.input_token != inputToken_) {
            next.inputSeen_ = true;
            next.inputToken_ = message.input_token;
            if (active_)
                next.active_ = false;
            else if (descending_ && now >= reportAt_ && now - reportAt_ < 500)
                next.active_ = true;
        }
    }
    if (next.active_ != active_) {
        Buffer command;
        if (!GliderCommand(ownedAgent, sequence, next.active_, command))
            return RelayResult::Invalid;
        *this = next;
        out = std::move(command);
        return RelayResult::Encoded;
    }
    *this = next;
    return RelayResult::Ignored;
}
} // namespace hot49284
} // namespace glide
