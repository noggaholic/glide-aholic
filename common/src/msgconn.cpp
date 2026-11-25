#include "glide/msgconn.h"

namespace glide {
DecodeStatus DecodeInit(ByteView input, InitHeader& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    if (input.size < 16)
        return DecodeStatus::Incomplete;
    Reader reader(input);
    InitHeader header;
    uint32_t extra = 0;
    if (!reader.U16(header.a) || !reader.U16(header.b) || !reader.U32(header.build) || !reader.U32(header.program) ||
        !reader.U32(extra))
        return DecodeStatus::Malformed;
    if (extra < 4)
        return DecodeStatus::Malformed;
    const size_t extensionSize = extra - 4;
    if (extensionSize != 0 && extensionSize != 68)
        return DecodeStatus::Malformed;
    if (!reader.Bytes(extensionSize, header.extension))
        return DecodeStatus::Incomplete;
    header.consumed = reader.Position();
    out = header;
    return DecodeStatus::Complete;
}
bool EncodeRawFrame(ByteView payload, Buffer& out) {
    if (!payload.Valid() || payload.size > UINT16_MAX)
        return false;
    Buffer frame;
    if (!frame.U16(0) || !frame.U16(static_cast<uint16_t>(payload.size)) || !frame.Append(payload))
        return false;
    return out.Append(frame.View());
}
} // namespace glide
