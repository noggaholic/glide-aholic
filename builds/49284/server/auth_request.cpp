#include "auth_request.h"
#include "glide/files.h"
#include <utility>
namespace glide {
namespace hot49284 {
namespace {
DecodeStatus Text(Reader& r, size_t capacity, std::string& out) {
    std::wstring text;
    for (size_t i = 0; i < capacity; ++i) {
        uint16_t unit;
        if (!r.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit)
            return WideToUtf8(text, out) ? DecodeStatus::Complete : DecodeStatus::Malformed;
        text.push_back(static_cast<wchar_t>(unit));
    }
    return DecodeStatus::Malformed;
}
DecodeStatus Fields(Reader& r, AuthRequest& m) {
    DecodeStatus status;
    std::string ignored;
    switch (m.id) {
    case 1:
    case 11:
    case 25:
        m.operation = m.id == 1 ? AuthOperation::Clock : m.id == 11 ? AuthOperation::Logout : AuthOperation::Roster;
        return r.VarU32(m.request);
    case 2:
        m.operation = AuthOperation::Machine;
        status = Text(r, 32, ignored);
        return status == DecodeStatus::Complete ? Text(r, 32, ignored) : status;
    case 3:
    case 10:
        m.operation = m.id == 3 ? AuthOperation::Identity : AuthOperation::Login;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        if (!r.Bytes(m.id == 3 ? 16 : 32, m.identity))
            return DecodeStatus::Incomplete;
        return m.id == 3 ? DecodeStatus::Complete : Text(r, 64, ignored);
    case 4: {
        m.operation = AuthOperation::Content;
        uint8_t size;
        if (!r.U8(size))
            return DecodeStatus::Incomplete;
        if (size > 96)
            return DecodeStatus::Malformed;
        return r.Bytes(size, m.data) && r.Bytes(16, m.identity) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 12: {
        m.operation = AuthOperation::Config;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        uint16_t size;
        if (!r.U16(size))
            return DecodeStatus::Incomplete;
        if (size > 4096)
            return DecodeStatus::Malformed;
        return r.Bytes(size, m.data) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 14:
        m.operation = AuthOperation::CurrentWorld;
        status = r.VarU32(m.request);
        if (status != DecodeStatus::Complete)
            return status;
        status = r.VarU32(m.world);
        if (status != DecodeStatus::Complete)
            return status;
        if (!r.U8(m.temporary))
            return DecodeStatus::Incomplete;
        return m.temporary <= 1 ? DecodeStatus::Complete : DecodeStatus::Malformed;
    case 15:
        m.operation = AuthOperation::Agreement;
        return r.U8(m.agreement) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    case 20:
    case 22:
        m.operation = m.id == 20 ? AuthOperation::Delete : AuthOperation::Select;
        status = r.VarU32(m.request);
        return status == DecodeStatus::Complete ? Text(r, 20, m.name) : status;
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
        return r.Bytes(16, m.identity) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    case 34:
        m.operation = AuthOperation::SystemReport;
        return r.VarU32(m.request);
    case 35:
        m.operation = AuthOperation::ViewLeave;
        return DecodeStatus::Complete;
    case 30:
        // Unlike June/July, this native record has no sequence or payload.
        m.operation = AuthOperation::KeepAlive;
        return DecodeStatus::Complete;
    default:
        return DecodeStatus::Unknown;
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
    out = std::move(m);
    return DecodeStatus::Complete;
}
} // namespace hot49284
} // namespace glide
