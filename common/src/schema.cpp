#include "glide/schema.h"

namespace glide {
namespace {
constexpr size_t kMaximumPayload = 2 * 1024 * 1024;
DecodeStatus Skip(Reader& reader, size_t bytes) {
    if (reader.Position() > kMaximumPayload || bytes > kMaximumPayload - reader.Position())
        return DecodeStatus::Malformed;
    ByteView ignored;
    return reader.Bytes(bytes, ignored) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
}
DecodeStatus Scan(Reader& reader, const SchemaField* fields, size_t count, unsigned depth, size_t& budget) {
    if ((!fields && count) || depth > 16 || count > budget)
        return DecodeStatus::Malformed;
    for (size_t i = 0; i < count; ++i) {
        if (!budget)
            return DecodeStatus::Malformed;
        --budget;
        const auto& field = fields[i];
        DecodeStatus status = DecodeStatus::Complete;
        uint32_t value;
        uint16_t word;
        uint8_t byte;
        switch (field.kind) {
        case FieldKind::U8:
            status = Skip(reader, 1);
            break;
        case FieldKind::U16:
            status = Skip(reader, 2);
            break;
        case FieldKind::U32:
            status = Skip(reader, 4);
            break;
        case FieldKind::Raw:
            status = Skip(reader, field.extent);
            break;
        case FieldKind::Var:
            status = reader.VarU32(value);
            break;
        case FieldKind::Len8:
            if (!reader.U8(byte))
                return DecodeStatus::Incomplete;
            status = Skip(reader, byte);
            break;
        case FieldKind::Len16:
            if (!reader.U16(word))
                return DecodeStatus::Incomplete;
            status = Skip(reader, word);
            break;
        case FieldKind::Guid:
            status = Skip(reader, 12);
            if (status == DecodeStatus::Complete)
                status = reader.VarU32(value);
            break;
        case FieldKind::Nest:
            if (!field.children || !field.childCount)
                return DecodeStatus::Malformed;
            if (!reader.U8(byte))
                return DecodeStatus::Incomplete;
            // The native schema uses a presence byte, not a count of repeated structures.
            if (byte)
                status = Scan(reader, field.children, field.childCount, depth + 1, budget);
            break;
        case FieldKind::Array8:
            if (!field.children || !field.childCount || field.extent > 255)
                return DecodeStatus::Malformed;
            if (!reader.U8(byte))
                return DecodeStatus::Incomplete;
            if (byte > field.extent)
                return DecodeStatus::Malformed;
            for (unsigned item = 0; item < byte; ++item) {
                status = Scan(reader, field.children, field.childCount, depth + 1, budget);
                if (status != DecodeStatus::Complete)
                    return status;
            }
            break;
        case FieldKind::Utf16Z: {
            const size_t limit = field.extent ? field.extent : 65535;
            if (limit > kMaximumPayload / 2)
                return DecodeStatus::Malformed;
            for (size_t units = 0;; ++units) {
                if (!reader.U16(word))
                    return DecodeStatus::Incomplete;
                if (!word)
                    break;
                if (units == limit)
                    return DecodeStatus::Malformed;
            }
            break;
        }
        default:
            return DecodeStatus::Malformed;
        }
        if (status != DecodeStatus::Complete)
            return status;
        if (reader.Position() > kMaximumPayload)
            return DecodeStatus::Malformed;
    }
    return DecodeStatus::Complete;
}
} // namespace
DecodeStatus SchemaSize(const SchemaField* fields, size_t count, ByteView input, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    size_t budget = 4096;
    const auto status = Scan(reader, fields, count, 0, budget);
    if (status == DecodeStatus::Complete)
        consumed = reader.Position();
    return status;
}
} // namespace glide
