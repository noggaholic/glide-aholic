#include "auth_request.h"

namespace glide {
namespace china167409 {
namespace {
struct NativeField {
    unsigned kind;
    size_t capacity;
};
struct NativeSchema {
    uint16_t id;
    const NativeField* fields;
    size_t count;
};
#include "auth_schemas.inc"

DecodeStatus Text(Reader& reader, size_t capacity) {
    bool surrogate = false;
    for (size_t units = 0; units < capacity; ++units) {
        uint16_t word;
        if (!reader.U16(word))
            return DecodeStatus::Incomplete;
        if (!word)
            return surrogate ? DecodeStatus::Malformed : DecodeStatus::Complete;
        if (units + 1 == capacity)
            return DecodeStatus::Malformed;
        if (surrogate) {
            if (word < 0xdc00 || word > 0xdfff)
                return DecodeStatus::Malformed;
            surrogate = false;
        } else {
            if (word >= 0xdc00 && word <= 0xdfff)
                return DecodeStatus::Malformed;
            surrogate = word >= 0xd800 && word <= 0xdbff;
        }
    }
    return DecodeStatus::Malformed;
}
DecodeStatus Field(Reader& reader, const NativeField& field) {
    uint8_t byte;
    uint16_t word;
    uint32_t compact;
    ByteView bytes;
    size_t count;
    switch (field.kind) {
    case 2:
        return reader.U8(byte) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    case 4:
        return reader.VarU32(compact);
    case 6:
        count = 4;
        break;
    case 11:
        count = 16;
        break;
    case 13:
        return Text(reader, field.capacity);
    case 19:
        count = field.capacity;
        break;
    case 20:
        if (!reader.U8(byte))
            return DecodeStatus::Incomplete;
        count = byte;
        if (count > field.capacity)
            return DecodeStatus::Malformed;
        break;
    case 21:
        if (!reader.U16(word))
            return DecodeStatus::Incomplete;
        count = word;
        if (count > field.capacity)
            return DecodeStatus::Malformed;
        break;
    default:
        return DecodeStatus::Malformed;
    }
    return reader.Bytes(count, bytes) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
}
} // namespace
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    for (const auto& schema : schemas)
        if (schema.id == id) {
            for (size_t i = 0; i < schema.count; ++i) {
                const auto status = Field(reader, schema.fields[i]);
                if (status != DecodeStatus::Complete)
                    return status;
            }
            const auto consumed = reader.Position();
            out = AuthRequest{id, consumed, ByteView(input.data + 2, consumed - 2)};
            return DecodeStatus::Complete;
        }
    return DecodeStatus::Unknown;
}
} // namespace china167409
} // namespace glide
