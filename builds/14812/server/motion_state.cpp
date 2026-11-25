#include "motion_state.h"
#include "motion.h"
#include "world_entry.h"
#include <cmath>
#include <algorithm>
namespace glide {
namespace july14812 {
PositionResult MotionPosition(ByteView input, CatalogPosition& out) {
    MotionMessage m;
    size_t consumed = 0;
    const auto status = DecodeMotion(input, m, consumed);
    if (status == DecodeStatus::Unknown)
        return PositionResult::Absent;
    if (status != DecodeStatus::Complete || consumed != input.size)
        return PositionResult::Invalid;
    if (m.id != 4 && (m.id < 6 || m.id > 8) && m.id != 10 && m.id != 13 && m.id != 16)
        return PositionResult::Absent;
    // Original July handler stores these client fields as physics coordinates.
    out = {m.position[0], m.position[1], m.position[2]};
    return PositionResult::Present;
}
FacingResult MotionFacing(ByteView input, Facing& out) {
    MotionMessage m;
    size_t consumed = 0;
    const auto status = DecodeMotion(input, m, consumed);
    if (status == DecodeStatus::Unknown)
        return FacingResult::Absent;
    if (status != DecodeStatus::Complete || consumed != input.size)
        return FacingResult::Invalid;
    if ((m.id != 8 && m.id != 9) || m.words[0] == 0x7fff || m.words[1] == 0x7fff)
        return FacingResult::Absent;
    const auto angle = [](uint16_t value) {
        const int word = value < 0x8000 ? value : static_cast<int>(value) - 65536;
        return word * 3.14159265358979323846 / 32766;
    };
    const double pitch = angle(m.words[0]), yaw = angle(m.words[1]);
    out = {static_cast<float>(std::cos(pitch) * std::cos(yaw)), static_cast<float>(std::cos(pitch) * std::sin(yaw)),
           static_cast<float>(std::sin(pitch))};
    return FacingResult::Present;
}
bool ValidOwnedMotion(ByteView input, uint16_t ownedAgent) {
    MotionMessage m;
    size_t consumed = 0;
    return ownedAgent && DecodeMotion(input, m, consumed) == DecodeStatus::Complete && consumed == input.size &&
           (m.id == 12 || m.id == 13 || m.agent == ownedAgent);
}
RelayResult OwnMotion(ByteView input, uint16_t ownedAgent, uint32_t sequence, Buffer& out) {
    MotionMessage m;
    size_t consumed = 0;
    const auto status = DecodeMotion(input, m, consumed);
    if (status == DecodeStatus::Unknown)
        return RelayResult::Ignored;
    if (status != DecodeStatus::Complete || consumed != input.size || !ownedAgent ||
        (m.id != 12 && m.id != 13 && m.agent != ownedAgent))
        return RelayResult::Invalid;
    if (m.id == 12 || m.id == 13)
        return RelayResult::Ignored;
    return sequence && EncodeMotion(m, ownedAgent, sequence, out) ? RelayResult::Encoded : RelayResult::Invalid;
}
bool SyncPosition(const CatalogPosition& point, uint16_t agent, uint32_t sequence, Buffer& out) {
    std::array<float, 3> native;
    if (!agent || !sequence || !NativePosition(point.x, point.y, point.z, native))
        return false;
    MotionMessage m;
    m.id = 16;
    m.state = 0x40;
    std::copy(native.begin(), native.end(), m.position);
    return EncodeMotion(m, agent, sequence, out);
}
} // namespace july14812
} // namespace glide
