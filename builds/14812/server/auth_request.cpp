#include "auth_request.h"
#include "auth_bytes.h"
#include <utility>
namespace glide {
namespace july14812 {
namespace {
enum class Kind { Var, Bytes, Text, Blob8, Blob16, Byte };
struct Field {
    Kind kind;
    uint16_t limit;
};
struct Record {
    uint16_t id;
    uint8_t count;
    Field fields[8];
};
#define V {Kind::Var, 0}
#define B(n) {Kind::Bytes, n}
#define S(n) {Kind::Text, n}
#define L8(n) {Kind::Blob8, n}
#define L16(n) {Kind::Blob16, n}
const Record records[] = {{1, 1, {V}},
                          {2, 2, {S(32), S(32)}},
                          {3, 2, {V, B(16)}},
                          {4, 2, {L8(96), B(16)}},
                          {5, 1, {V}},
                          {6, 3, {V, V, S(26)}},
                          {7, 4, {V, B(16), S(64), S(26)}},
                          {8, 1, {V}},
                          {9, 8, {V, V, V, V, V, V, V, S(64)}},
                          {10, 4, {V, B(16), B(16), S(64)}},
                          {11, 1, {V}},
                          {12, 2, {V, V}},
                          {13, 2, {V, L16(512)}},
                          {14, 2, {V, V}},
                          {15, 1, {{Kind::Byte, 0}}},
                          {16, 3, {V, V, V}},
                          {17, 8, {V, S(28), S(28), S(32), S(32), S(12), S(28), S(28)}},
                          {18, 1, {V}},
                          {19, 1, {V}},
                          {20, 2, {V, S(20)}},
                          {21, 3, {V, B(16), S(20)}},
                          {22, 2, {V, S(20)}},
                          {23, 3, {V, S(20), L8(512)}},
                          {24, 3, {V, S(20), S(20)}},
                          {25, 1, {V}},
                          {26, 1, {V}},
                          {27, 1, {V}},
                          {28, 4, {V, V, V, V}},
                          {29, 5, {V, V, V, V, V}},
                          {30, 1, {V}},
                          {31, 1, {V}},
                          {32, 5, {V, V, S(64), S(64), S(256)}},
                          {33, 2, {V, S(26)}}};
#undef V
#undef B
#undef S
#undef L8
#undef L16
DecodeStatus Registered(Reader& r, AuthRequest& m) {
    const Record* record = nullptr;
    for (const auto& candidate : records)
        if (candidate.id == m.id) {
            record = &candidate;
            break;
        }
    if (!record)
        return DecodeStatus::Unknown;
    std::string ignored;
    ByteView bytes;
    uint32_t value = 0;
    uint8_t small = 0;
    uint16_t size = 0;
    for (unsigned i = 0; i < record->count; ++i) {
        const auto& field = record->fields[i];
        DecodeStatus status;
        switch (field.kind) {
        case Kind::Var:
            status = r.VarU32(value);
            if (status != DecodeStatus::Complete)
                return status;
            if (!i)
                m.request = value;
            break;
        case Kind::Bytes:
            if (!r.Bytes(field.limit, bytes))
                return DecodeStatus::Incomplete;
            break;
        case Kind::Text:
            status = detail::Text(r, field.limit, ignored);
            if (status != DecodeStatus::Complete)
                return status;
            break;
        case Kind::Blob8:
            if (!r.U8(small))
                return DecodeStatus::Incomplete;
            if (small > field.limit)
                return DecodeStatus::Malformed;
            if (!r.Bytes(small, bytes))
                return DecodeStatus::Incomplete;
            break;
        case Kind::Blob16:
            if (!r.U16(size))
                return DecodeStatus::Incomplete;
            if (size > field.limit)
                return DecodeStatus::Malformed;
            if (!r.Bytes(size, bytes))
                return DecodeStatus::Incomplete;
            break;
        case Kind::Byte:
            if (!r.U8(small))
                return DecodeStatus::Incomplete;
            break;
        }
    }
    return DecodeStatus::Complete;
}
DecodeStatus Fields(Reader& r, AuthRequest& m) {
    DecodeStatus status;
    std::string ignored;
    switch (m.id) {
    case 1:
    case 11:
    case 30:
        m.operation = m.id == 1    ? AuthOperation::Clock
                      : m.id == 11 ? AuthOperation::Reauthenticate
                                   : AuthOperation::Sequence;
        return r.VarU32(m.request);
    case 2:
        m.operation = AuthOperation::Machine;
        status = detail::Text(r, 32, ignored);
        return status == DecodeStatus::Complete ? detail::Text(r, 32, ignored) : status;
    case 3:
    case 10:
        m.operation = m.id == 3 ? AuthOperation::Identity : AuthOperation::Login;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        if (!r.Bytes(m.id == 3 ? 16 : 32, m.identity))
            return DecodeStatus::Incomplete;
        return m.id == 3 ? DecodeStatus::Complete : detail::Text(r, 64, ignored);
    case 4: {
        m.operation = AuthOperation::Content;
        uint8_t size;
        if (!r.U8(size))
            return DecodeStatus::Incomplete;
        if (size > 96)
            return DecodeStatus::Malformed;
        return r.Bytes(size, m.data) && r.Bytes(16, m.identity) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 12:
        m.operation = AuthOperation::ConfigBegin;
        status = r.VarU32(m.request);
        return status == DecodeStatus::Complete ? r.VarU32(m.totalBytes) : status;
    case 13: {
        m.operation = AuthOperation::ConfigChunk;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        uint16_t size;
        if (!r.U16(size))
            return DecodeStatus::Incomplete;
        if (size > 512)
            return DecodeStatus::Malformed;
        return r.Bytes(size, m.data) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 25:
        m.operation = AuthOperation::Roster;
        return r.VarU32(m.request);
    case 14:
        m.operation = AuthOperation::CurrentWorld;
        status = r.VarU32(m.request);
        return status == DecodeStatus::Complete ? r.VarU32(m.world) : status;
    case 15:
        m.operation = AuthOperation::Agreement;
        return r.U8(m.agreement) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    case 20:
    case 22:
        m.operation = m.id == 20 ? AuthOperation::Delete : AuthOperation::Select;
        status = r.VarU32(m.request);
        return status == DecodeStatus::Complete ? detail::Text(r, 20, m.name) : status;
    case 29:
        m.operation = AuthOperation::Redirect;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        for (auto& route : m.routing) {
            status = r.VarU32(route);
            if (status != DecodeStatus::Complete)
                return status;
        }
        return DecodeStatus::Complete;
    default:
        return Registered(r, m);
    }
}
} // namespace
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader r(input);
    AuthRequest m;
    if (!r.U16(m.id))
        return DecodeStatus::Incomplete;
    const auto status = Fields(r, m);
    if (status != DecodeStatus::Complete)
        return status;
    m.consumed = r.Position();
    if (m.operation == AuthOperation::Unclassified)
        m.data = ByteView(input.data + 2, m.consumed - 2);
    out = std::move(m);
    return DecodeStatus::Complete;
}
} // namespace july14812
} // namespace glide
