#include "control_request.h"
namespace glide {
namespace hot49284 {
namespace {
struct Field {
    unsigned type, limit;
};
DecodeStatus Skip(Reader& reader, Field field) {
    uint32_t value = 0;
    ByteView bytes;
    if (field.type == 4)
        return reader.VarU32(value);
    if (field.type == 13) {
        for (unsigned i = 0; i < field.limit; ++i) {
            uint16_t unit;
            if (!reader.U16(unit))
                return DecodeStatus::Incomplete;
            if (!unit)
                return DecodeStatus::Complete;
        }
        return DecodeStatus::Malformed;
    }
    if (field.type == 17) {
        uint8_t count;
        if (!reader.U8(count))
            return DecodeStatus::Incomplete;
        if (count > field.limit)
            return DecodeStatus::Malformed;
        for (unsigned i = 0; i < count; ++i) {
            const auto status = reader.VarU32(value);
            if (status != DecodeStatus::Complete)
                return status;
        }
        return DecodeStatus::Complete;
    }
    return reader.Bytes(field.type == 11  ? 16
                        : field.type == 3 ? 2
                                          : 1,
                        bytes)
               ? DecodeStatus::Complete
               : DecodeStatus::Incomplete;
}
} // namespace
bool IsSharedControl(uint16_t id) {
    return id == 1 || id == 2 || (id >= 163 && id <= 166) || id == 256 || id == 257;
}
DecodeStatus DecodeControl(ByteView input, uint16_t& outputId, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    static const Field info[] = {{2, 0}, {4, 0}, {3, 0}, {13, 13}, {4, 0},  {13, 128}, {3, 0},
                                 {4, 0}, {4, 0}, {4, 0}, {3, 0},   {4, 0},  {3, 0},    {3, 0},
                                 {3, 0}, {3, 0}, {3, 0}, {2, 0},   {11, 0}, {13, 256}};
    static const Field start[] = {{13, 64}, {13, 16}, {2, 0}, {2, 0}};
    static const Field periodic[] = {{4, 0}}, modules[] = {{4, 0}, {17, 32}, {11, 0}, {2, 0}};
    static const Field text[] = {{13, 1024}}, counters[] = {{4, 0}, {11, 0}, {4, 0}, {4, 0}, {4, 0}};
    const Field* fields = nullptr;
    size_t count = 0;
    switch (id) {
    case 1:
        break;
    case 257:
        fields = start;
        count = 4;
        break;
    case 2:
        fields = periodic;
        count = 1;
        break;
    case 163:
        fields = info;
        count = 20;
        break;
    case 164:
        fields = modules;
        count = 4;
        break;
    case 165:
        fields = text;
        count = 1;
        break;
    case 166:
        fields = counters;
        count = 5;
        break;
    case 256: {
        uint8_t a, b;
        if (!reader.U8(a) || !reader.U8(b))
            return DecodeStatus::Incomplete;
        if (a > 5 || b > 5)
            return DecodeStatus::Malformed;
        break;
    }
    default:
        return DecodeStatus::Unknown;
    }
    for (size_t i = 0; i < count; ++i) {
        const auto status = Skip(reader, fields[i]);
        if (status != DecodeStatus::Complete)
            return status;
    }
    outputId = id;
    consumed = reader.Position();
    return DecodeStatus::Complete;
}
} // namespace hot49284
} // namespace glide
