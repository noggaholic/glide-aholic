#include "motion.h"
#include <utility>
namespace glide {
namespace hot49284 {
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    if (input.size < 2)
        return DecodeStatus::Incomplete;
    const uint16_t id = static_cast<uint16_t>(input.data[0] | (uint16_t(input.data[1]) << 8));
    if (id < 3 || id > 20 || id == 10)
        return DecodeStatus::Unknown;
    MotionMessage value{};
    size_t size = 0;
    const int status = glide_49284_agent_decode(input.data, input.size, &value, &size);
    if (status != 1)
        return status == 0 ? DecodeStatus::Incomplete : DecodeStatus::Malformed;
    out = value;
    consumed = size;
    return DecodeStatus::Complete;
}
bool RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out) {
    MotionMessage message{};
    size_t consumed = 0, written = 0;
    uint8_t bytes[240];
    if (DecodeMotion(input, message, consumed) != DecodeStatus::Complete || consumed != input.size ||
        !glide_49284_agent_encode(&message, agent, sequence, bytes, sizeof(bytes), &written))
        return false;
    Buffer result;
    if (!result.Append(ByteView(bytes, written)))
        return false;
    out = std::move(result);
    return true;
}
} // namespace hot49284
} // namespace glide
