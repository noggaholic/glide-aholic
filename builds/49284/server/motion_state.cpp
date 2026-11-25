#include "motion_state.h"
#include "motion.h"
#include "world_entry.h"
#include <cmath>
namespace glide {
namespace hot49284 {
PositionResult MotionPosition(ByteView input, CatalogPosition& out) {
    MotionMessage message{};
    size_t consumed = 0;
    const auto status = DecodeMotion(input, message, consumed);
    if (status == DecodeStatus::Unknown)
        return PositionResult::Absent;
    if (status != DecodeStatus::Complete || consumed != input.size)
        return PositionResult::Invalid;
    if (!message.has_position)
        return PositionResult::Absent;
    const float scale = message.id == 19 ? 1.0f / 32 : 1.0f;
    out = {message.position[0] * scale, message.position[1] * scale,
           message.position[2] == 0 ? 0 : message.position[2] * (message.id == 19 ? -scale : scale)};
    return PositionResult::Present;
}
FacingResult MotionFacing(ByteView input, Facing& out) {
    MotionMessage message{};
    size_t consumed = 0;
    const auto status = DecodeMotion(input, message, consumed);
    if (status == DecodeStatus::Unknown)
        return FacingResult::Absent;
    if (status != DecodeStatus::Complete || consumed != input.size)
        return FacingResult::Invalid;
    if (message.id != 9 || message.words[0] == 0x7fff || message.words[1] == 0x7fff)
        return FacingResult::Absent;
    const int word = message.words[1] < 0x8000 ? message.words[1] : static_cast<int>(message.words[1]) - 65536;
    const double yaw = word * 3.14159265358979323846 / 32766;
    out = {static_cast<float>(std::cos(yaw)), static_cast<float>(std::sin(yaw)), 0};
    return FacingResult::Present;
}
RelayResult OwnMotion(ByteView input, uint16_t ownedAgent, uint32_t sequence, Buffer& out) {
    MotionMessage message{};
    size_t consumed = 0;
    const auto status = DecodeMotion(input, message, consumed);
    if (status == DecodeStatus::Unknown)
        return RelayResult::Ignored;
    if (status != DecodeStatus::Complete || consumed != input.size || !ownedAgent ||
        (message.id != 15 && message.id != 16 && message.agent != ownedAgent))
        return RelayResult::Invalid;
    if (message.id == 5 || message.id == 15 || message.id == 16)
        return RelayResult::Ignored;
    if (!sequence || !RelayMotion(input, ownedAgent, sequence, out))
        return RelayResult::Invalid;
    return RelayResult::Encoded;
}
bool SyncPosition(const CatalogPosition& position, uint16_t agent, uint32_t sequence, Buffer& out) {
    std::array<float, 3> native;
    if (!agent || !sequence || !NativePosition(position.x, position.y, position.z, native))
        return false;
    Buffer result;
    if (!result.U16(67) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(0x50))
        return false;
    for (float value : native)
        if (!result.F32(value))
            return false;
    if (!result.VarU32(0))
        return false;
    out = std::move(result);
    return true;
}
} // namespace hot49284
} // namespace glide
