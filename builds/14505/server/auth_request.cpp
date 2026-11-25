#include "auth_request.h"
#include "glide/files.h"
#include <utility>

namespace glide {
namespace june14505 {
namespace {
DecodeStatus Text(Reader& reader, size_t capacity, std::string& out) {
    std::wstring value;
    for (size_t i = 0; i < capacity; ++i) {
        uint16_t unit;
        if (!reader.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit)
            return WideToUtf8(value, out) ? DecodeStatus::Complete : DecodeStatus::Malformed;
        value.push_back(static_cast<wchar_t>(unit));
    }
    return DecodeStatus::Malformed;
}
DecodeStatus Fields(Reader& reader, AuthRequest& result, uint16_t id) {
    DecodeStatus status;
    std::string ignored;
    switch (id) {
    case 1:
    case 11:
    case 30:
        result.operation = id == 1    ? AuthOperation::Clock
                           : id == 11 ? AuthOperation::Reauthenticate
                                      : AuthOperation::Sequence;
        return reader.VarU32(result.request);
    case 2:
        result.operation = AuthOperation::Machine;
        status = Text(reader, 32, ignored);
        return status == DecodeStatus::Complete ? Text(reader, 32, ignored) : status;
    case 3:
    case 10:
        result.operation = id == 3 ? AuthOperation::Identity : AuthOperation::Login;
        status = reader.VarU32(result.request);
        if (status != DecodeStatus::Complete)
            return status;
        if (!reader.Bytes(id == 3 ? 16 : 32, result.identity))
            return DecodeStatus::Incomplete;
        return id == 3 ? DecodeStatus::Complete : Text(reader, 64, ignored);
    case 4: {
        result.operation = AuthOperation::Content;
        uint8_t size;
        if (!reader.U8(size))
            return DecodeStatus::Incomplete;
        if (size > 92)
            return DecodeStatus::Malformed;
        return reader.Bytes(size, result.data) && reader.Bytes(16, result.identity) ? DecodeStatus::Complete
                                                                                    : DecodeStatus::Incomplete;
    }
    case 20:
    case 22:
        result.operation = id == 20 ? AuthOperation::Delete : AuthOperation::Select;
        status = reader.VarU32(result.request);
        return status == DecodeStatus::Complete ? Text(reader, 20, result.name) : status;
    case 12:
        result.operation = AuthOperation::ConfigBegin;
        status = reader.VarU32(result.request);
        return status == DecodeStatus::Complete ? reader.VarU32(result.totalBytes) : status;
    case 13: {
        result.operation = AuthOperation::ConfigChunk;
        status = reader.VarU32(result.request);
        if (status != DecodeStatus::Complete)
            return status;
        uint16_t size;
        if (!reader.U16(size))
            return DecodeStatus::Incomplete;
        if (size > 512)
            return DecodeStatus::Malformed;
        return reader.Bytes(size, result.data) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    }
    case 29:
        result.operation = AuthOperation::Redirect;
        status = reader.VarU32(result.request);
        for (auto& route : result.routing) {
            if (status != DecodeStatus::Complete)
                return status;
            status = reader.VarU32(route);
        }
        return status;
    case 15:
        result.operation = AuthOperation::Agreement;
        return reader.U8(result.agreement) ? DecodeStatus::Complete : DecodeStatus::Incomplete;
    default:
        return DecodeStatus::Unknown;
    }
}
} // namespace
DecodeStatus DecodeAuthRequest(ByteView input, AuthRequest& out) {
    return DecodeAuthRequestForBuild(input, 14505, out);
}
DecodeStatus DecodeAuthRequestForBuild(ByteView input, unsigned build, AuthRequest& out) {
    if (build != 14075 && build != 14422 && build != 14505)
        return DecodeStatus::Unknown;
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    AuthRequest result;
    if (!reader.U16(result.id))
        return DecodeStatus::Incomplete;
    uint16_t canonical = result.id;
    if (build == 14075) {
        const uint16_t ids[][2] = {{0, 1},   {1, 2},   {2, 3},   {5, 10},  {7, 20},  {15, 4},  {12, 11},
                                   {35, 30}, {10, 22}, {33, 12}, {32, 13}, {41, 29}, {38, 15}, {29, 29}};
        canonical = UINT16_MAX;
        for (const auto& pair : ids)
            if (pair[0] == result.id) {
                canonical = pair[1];
                break;
            }
    }
    const auto status = Fields(reader, result, canonical);
    if (status != DecodeStatus::Complete)
        return status;
    if (build == 14075 && result.id == 0x001d)
        result.operation = AuthOperation::LegacyControl;
    result.consumed = reader.Position();
    out = std::move(result);
    return DecodeStatus::Complete;
}
bool SessionReady(uint32_t connection, Buffer& out) {
    Buffer result;
    if (!result.U16(2) || !result.VarU32(connection))
        return false;
    out = std::move(result);
    return true;
}
} // namespace june14505
} // namespace glide
