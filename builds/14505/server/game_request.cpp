#include "game_request.h"
#include "world_protocol.h"
#include "glide/files.h"

namespace glide {
namespace june14505 {
DecodeStatus DecodeGameRequestForBuild(ByteView input, unsigned build, GameRequest& out) {
    if (!WorldProtocolBuild(build))
        return DecodeStatus::Unknown;
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    GameRequest request;
    if (!reader.U16(request.id))
        return DecodeStatus::Incomplete;
    uint16_t id = request.id;
    const bool aprilServerCommand = build == 14075 && id == 0x004c;
    if (build == 14075) {
        const uint16_t pairs[][2] = {{0x48, 0x53}, {0x49, 0x54}, {0x4c, 0x54}, {0x3d, 0x44}, {0x4f, 0x59}};
        id = UINT16_MAX;
        for (const auto& pair : pairs)
            if (pair[0] == request.id) {
                id = pair[1];
                break;
            }
    }
    DecodeStatus status;
    switch (id) {
    case 0x0053:
    case 0x0054: {
        request.kind = id == 0x0053 ? GameRequestKind::ChatTarget : GameRequestKind::ChatChannel;
        if (id == 0x0053) {
            status = reader.VarU32(request.value);
            if (status != DecodeStatus::Complete)
                return status;
        }
        std::wstring wide;
        for (;;) {
            uint16_t unit;
            if (!reader.U16(unit))
                return DecodeStatus::Incomplete;
            if (!unit)
                break;
            if (wide.size() == 200)
                return DecodeStatus::Malformed;
            wide += static_cast<wchar_t>(unit);
        }
        if (!WideToUtf8(wide, request.text))
            return DecodeStatus::Malformed;
        if (aprilServerCommand) {
            // Native April general slash-command relay, including /map.
            // Decode its own compact field; classification happens above framing.
            status = reader.VarU32(request.value);
            if (status != DecodeStatus::Complete)
                return status;
            request.kind = GameRequestKind::ServerCommand;
        } else if (id == 0x0054 && !reader.U8(request.channel))
            return DecodeStatus::Incomplete;
        break;
    }
    case 0x0044:
    case 0x001c:
        request.kind = id == 0x0044 ? GameRequestKind::Waypoint : GameRequestKind::Hint;
        status = reader.VarU32(request.value);
        if (status != DecodeStatus::Complete)
            return status;
        if ((id == 0x0044 && !request.value) || (id == 0x001c && request.value > 255))
            return DecodeStatus::Malformed;
        break;
    case 0x0059: {
        request.kind = GameRequestKind::Debug;
        status = reader.VarU32(request.value);
        if (status != DecodeStatus::Complete)
            return status;
        uint8_t count;
        if (!reader.U8(count))
            return DecodeStatus::Incomplete;
        if (count > 16)
            return DecodeStatus::Malformed;
        for (uint8_t i = 0; i < count; ++i) {
            uint32_t value;
            status = reader.VarU32(value);
            if (status != DecodeStatus::Complete)
                return status;
            request.values.push_back(value);
        }
        break;
    }
    case 0x0013:
        request.kind = GameRequestKind::SkillbarRefresh;
        break;
    default:
        status = DecodeMotionForBuild(input, build, request.motion);
        if (status != DecodeStatus::Unknown) {
            if (status != DecodeStatus::Complete)
                return status;
            request.kind = GameRequestKind::Motion;
            request.consumed = request.motion.consumed;
        } else {
            status = DecodeMissionForBuild(input, build, request.mission);
            if (status != DecodeStatus::Complete)
                return status;
            request.kind = GameRequestKind::Mission;
            request.consumed = request.mission.consumed;
        }
        out = std::move(request);
        return DecodeStatus::Complete;
    }
    request.consumed = reader.Position();
    out = std::move(request);
    return DecodeStatus::Complete;
}
DecodeStatus DecodeGameRequest(ByteView input, GameRequest& out) {
    return DecodeGameRequestForBuild(input, 14505, out);
}
} // namespace june14505
} // namespace glide
