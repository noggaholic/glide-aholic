#include "motion.h"
#include "world_protocol.h"
#include <cstring>

namespace glide {
namespace june14505 {
namespace {
DecodeStatus Floats(Reader& reader, float* out, size_t count, bool positiveInfinity = false) {
    ByteView bytes;
    if (!reader.Bytes(count * 4, bytes))
        return DecodeStatus::Incomplete;
    for (size_t i = 0; i < count; ++i) {
        uint32_t bits;
        std::memcpy(&bits, bytes.data + i * 4, 4);
        if ((bits & 0x7f800000u) == 0x7f800000u && !(positiveInfinity && bits == 0x7f800000u))
            return DecodeStatus::Malformed;
        std::memcpy(out + i, &bits, 4);
    }
    return DecodeStatus::Complete;
}
} // namespace
DecodeStatus DecodeMotionForBuild(ByteView input, unsigned build, MotionMessage& out) {
    if (!WorldProtocolBuild(build))
        return DecodeStatus::Unknown;
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    MotionMessage message;
    if (!reader.U16(message.id))
        return DecodeStatus::Incomplete;
    uint16_t id = message.id;
    if (build == 14075) {
        const uint16_t pairs[][2] = {{0, 0}, {3, 3}, {4, 4},  {5, 5},   {6, 6},
                                     {7, 7}, {8, 8}, {10, 9}, {14, 13}, {16, 15}};
        id = UINT16_MAX;
        for (const auto& pair : pairs)
            if (pair[0] == message.id) {
                id = pair[1];
                break;
            }
    }
    if (id != 3 && id != 4 && id != 7 && id != 8 && id != 9 && id != 13 && id != 15 &&
        !(build == 14075 && (id == 0 || id == 5 || id == 6)))
        return DecodeStatus::Unknown;
    message.canonicalId = id;
    DecodeStatus status;
    if (id == 0) {
        if (!reader.U8(message.state))
            return DecodeStatus::Incomplete;
        status = Floats(reader, message.vectorA.data(), 3);
        if (status != DecodeStatus::Complete)
            return status;
        if (!reader.U16(message.optionalWord))
            return DecodeStatus::Incomplete;
    } else if (id == 0x000d) {
        status = reader.VarU32(message.sequence);
        if (status != DecodeStatus::Complete)
            return status;
        status = reader.VarU32(message.tick);
        if (status != DecodeStatus::Complete)
            return status;
        status = Floats(reader, message.position.data(), 4);
        if (status != DecodeStatus::Complete)
            return status;
        if (!reader.U8(message.state) || !reader.U8(message.mode))
            return DecodeStatus::Incomplete;
    } else {
        if (!reader.U16(message.agent))
            return DecodeStatus::Incomplete;
        if (!message.agent)
            return DecodeStatus::Malformed;
        status = reader.VarU32(message.sequence);
        if (status != DecodeStatus::Complete)
            return status;
        if (!reader.U8(message.state))
            return DecodeStatus::Incomplete;
        if (id != 3 && id != 4) {
            status = reader.VarU32(message.flags);
            if (status != DecodeStatus::Complete)
                return status;
            uint8_t count;
            if (!reader.U8(count))
                return DecodeStatus::Incomplete;
            if (count > 80)
                return DecodeStatus::Malformed;
            if (!reader.Bytes(count, message.input))
                return DecodeStatus::Incomplete;
        }
        if (id == 5) {
            if (!reader.U16(message.wordA))
                return DecodeStatus::Incomplete;
        } else if (id == 9) {
            status = Floats(reader, &message.amount, 1);
            if (status != DecodeStatus::Complete)
                return status;
            if (!reader.U32(message.token))
                return DecodeStatus::Incomplete;
        } else if (id != 3 && id != 15) {
            status = Floats(reader, message.position.data(), 4);
            if (status != DecodeStatus::Complete)
                return status;
            status = Floats(reader, message.vectorA.data(), 3);
            if (status != DecodeStatus::Complete)
                return status;
            if (id == 6) {
                if (!reader.U16(message.wordA) || !reader.U16(message.wordB) || !reader.U16(message.optionalWord))
                    return DecodeStatus::Incomplete;
            } else if (id == 8) {
                status = Floats(reader, message.vectorB.data(), 3);
                if (status != DecodeStatus::Complete)
                    return status;
                uint8_t optional;
                if (!reader.U16(message.wordA) || !reader.U16(message.wordB) || !reader.U8(optional))
                    return DecodeStatus::Incomplete;
                if (optional > 1)
                    return DecodeStatus::Malformed;
                message.hasOptional = optional != 0;
                if (message.hasOptional) {
                    if (!reader.U32(message.optionalValue) || !reader.U16(message.optionalWord))
                        return DecodeStatus::Incomplete;
                    // Native double-tap dodge uses +Infinity only in this optional vector.
                    status = Floats(reader, message.optionalVector.data(), 4, true);
                    if (status != DecodeStatus::Complete)
                        return status;
                }
            }
        }
    }
    message.consumed = reader.Position();
    out = message;
    return DecodeStatus::Complete;
}
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out) {
    return DecodeMotionForBuild(input, 14505, out);
}
} // namespace june14505
} // namespace glide
