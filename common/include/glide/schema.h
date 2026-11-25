#pragma once
#include "glide/buffer.h"

namespace glide {
enum class FieldKind { U8, U16, U32, Raw, Var, Len8, Len16, Guid, Nest, Array8, Utf16Z };
struct SchemaField {
    FieldKind kind;
    size_t extent = 0; // Raw bytes, Array8 maximum count, or UTF16 maximum units (zero uses 65535).
    const SchemaField* children = nullptr;
    size_t childCount = 0;
};
// Sizes one payload, retaining following messages. Only Complete changes consumed.
DecodeStatus SchemaSize(const SchemaField* fields, size_t count, ByteView input, size_t& consumed);
template <size_t N> DecodeStatus SchemaSize(const SchemaField (&fields)[N], ByteView input, size_t& consumed) {
    return SchemaSize(fields, N, input, consumed);
}
} // namespace glide
