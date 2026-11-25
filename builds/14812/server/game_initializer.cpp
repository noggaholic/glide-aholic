#include "game_initializer.h"
#include "auth_bytes.h"
#include <algorithm>
namespace glide {
namespace july14812 {
namespace {
class JulyInitializers final : public GameInitializerCodec {
  public:
    bool ValidAuth(const InitHeader& header) const override {
        return header.a == 0x0400 && header.b == 12 && header.build == 14812 && header.program == 101 &&
               header.consumed == 16 && header.extension.Valid() && !header.extension.size;
    }
    bool DecodeGame(const InitHeader& header, GameJoinRequest& out) const override {
        GameInitializer parsed;
        if (!DecodeGameInitializer(header, parsed) || !parsed.association || parsed.map > UINT16_MAX ||
            (!parsed.map && parsed.context))
            return false;
        GameJoinRequest result;
        result.association = parsed.association;
        result.map = parsed.map;
        result.context = parsed.context;
        std::copy(parsed.user.begin(), parsed.user.end(), result.user.bytes);
        std::copy(parsed.character.begin(), parsed.character.end(), result.character.bytes);
        out = result;
        return true;
    }
    bool Redirect(uint32_t association, uint16_t map, uint16_t port, Buffer& out) const override {
        if (!association || !map || !port)
            return false;
        uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
        endpoint[2] = static_cast<uint8_t>(port >> 8);
        endpoint[3] = static_cast<uint8_t>(port);
        Buffer reply;
        // July S483: endpoint, unused world, association, map, context, tail.
        // Bound world transfers use zero context; native handler forwards it to Join.
        if (!reply.U16(483) || !reply.Append(ByteView(endpoint, 28)) || !reply.VarU32(0) ||
            !reply.VarU32(association) || !reply.U16(map) || !reply.VarU32(0) || !reply.VarU32(0))
            return false;
        out = std::move(reply);
        return true;
    }
};
} // namespace
const GameInitializerCodec& Initializers() {
    static JulyInitializers codec;
    return codec;
}
const TransportVariant& Transport() {
    static const TransportVariant variant{101, false, KeyExchangeRecord::SizedAligned};
    return variant;
}
bool DecodeGameInitializer(const InitHeader& header, GameInitializer& out) {
    if (header.a != 0x0500 || header.b != 12 || header.build != 14812 || header.program != 101 ||
        header.consumed != 84 || !header.extension.Valid() || header.extension.size != 68)
        return false;
    Reader reader(header.extension);
    GameInitializer result;
    if (!reader.U32(result.association) || !reader.U32(result.unwritten) || !reader.U32(result.map) ||
        !reader.U32(result.context) || !detail::Read(reader, result.user) || !detail::Read(reader, result.character) ||
        !detail::Read(reader, result.client) || !reader.U32(result.clientContext) || reader.Remaining())
        return false;
    out = result;
    return true;
}
bool DecodeGameInitializer(ByteView bytes, GameInitializer& out) {
    if (!bytes.Valid() || bytes.size != 84)
        return false;
    InitHeader header;
    return DecodeInit(bytes, header) == DecodeStatus::Complete && DecodeGameInitializer(header, out);
}
} // namespace july14812
} // namespace glide
