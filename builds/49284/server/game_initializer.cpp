#include "game_initializer.h"
#include <algorithm>
namespace glide {
namespace hot49284 {
namespace {
class HotInitializers final : public GameInitializerCodec {
  public:
    bool Redirect(uint32_t association, uint16_t map, uint16_t port, Buffer& out) const override {
        if (!association || !map || !port)
            return false;
        uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
        endpoint[2] = static_cast<uint8_t>(port >> 8);
        endpoint[3] = static_cast<uint8_t>(port);
        Buffer reply;
        if (!reply.U16(754) || !reply.Append(ByteView(endpoint, 28)) || !reply.VarU32(0) ||
            !reply.VarU32(association) || !reply.U16(map) || !reply.VarU32(0) || !reply.VarU32(0))
            return false;
        out = std::move(reply);
        return true;
    }
    bool ValidAuth(const InitHeader& header) const override {
        return header.a == 0x0400 && header.b == 0x000c && header.build == 49284 && header.program == 101 &&
               header.consumed == 16 && header.extension.Valid() && !header.extension.size;
    }
    bool DecodeGame(const InitHeader& header, GameJoinRequest& out) const override {
        GameInitializer native;
        if (!DecodeGameInitializer(header, native) || !native.association || native.map > UINT16_MAX ||
            (!native.map && native.context))
            return false;
        GameJoinRequest request;
        request.association = native.association;
        request.map = native.map;
        request.context = native.context;
        std::copy(native.user.begin(), native.user.end(), request.user.bytes);
        std::copy(native.character.begin(), native.character.end(), request.character.bytes);
        out = request;
        return true;
    }
};
} // namespace
const GameInitializerCodec& Initializers() {
    static HotInitializers codec;
    return codec;
}
bool DecodeGameInitializer(const InitHeader& header, GameInitializer& out) {
    if (header.a != 0x0500 || header.b != 0x000c || header.build != 49284 || header.program != 101 ||
        header.consumed != 84 || !header.extension.Valid() || header.extension.size != 68)
        return false;
    Reader reader(header.extension);
    GameInitializer parsed;
    if (!reader.U32(parsed.association) || !reader.U32(parsed.unwritten) || !reader.U32(parsed.map) ||
        !reader.U32(parsed.context))
        return false;
    for (auto* guid : {&parsed.user, &parsed.character, &parsed.client}) {
        ByteView bytes;
        if (!reader.Bytes(16, bytes))
            return false;
        std::copy(bytes.data, bytes.data + 16, guid->begin());
    }
    if (!reader.U32(parsed.clientContext) || reader.Remaining())
        return false;
    out = parsed;
    return true;
}
bool DecodeGameInitializer(ByteView bytes, GameInitializer& out) {
    if (!bytes.Valid() || bytes.size != 84)
        return false;
    InitHeader header;
    return DecodeInit(bytes, header) == DecodeStatus::Complete && DecodeGameInitializer(header, out);
}
} // namespace hot49284
} // namespace glide
