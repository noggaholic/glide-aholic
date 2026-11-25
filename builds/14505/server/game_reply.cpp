#include "game_reply.h"
#include "world_protocol.h"
#include "glide/files.h"

namespace glide {
namespace june14505 {
bool ChatReply(const std::string& text, Buffer& out) {
    return ChatReplyForBuild(14505, text, out);
}
bool ChatReplyForBuild(unsigned build, const std::string& text, Buffer& out) {
    return ChatPublicationForBuild(build, text, "", Guid{}, 9, 0x11, out);
}
bool ChatPublicationForBuild(unsigned build, const std::string& text, const std::string& sender, const Guid& identity,
                             uint8_t channel, uint8_t display, Buffer& out) {
    if (!WorldProtocolBuild(build))
        return false;
    // At most 251 UTF-16 text units before escaping, plus five framing units.
    if (text.size() > 1004 || sender.size() > 288)
        return false;
    std::wstring wide, name;
    if (!Utf8ToWide(text, wide) || wide.size() > 251 || !Utf8ToWide(sender, name) || name.size() > 72)
        return false;
    size_t units = 5;
    for (wchar_t unit : wide)
        units += unit == L'[' || unit == L']' || unit == L'\\' ? 2 : 1;
    if (units > 256)
        return false;
    Buffer result;
    if (!result.U16(build == 14075 ? 0x00fc : 0x0133) || !result.U16(0x8101) || !result.U16(0x447b) ||
        !result.U16(0x0107))
        return false;
    for (wchar_t unit : wide) {
        if ((unit == L'[' || unit == L']' || unit == L'\\') && !result.U16('\\'))
            return false;
        if (!result.U16(static_cast<uint16_t>(unit)))
            return false;
    }
    if (!result.U16(1) || !result.U16(0) || !result.U16(build == 14075 ? 0x00fd : 0x0134) || !result.U8(channel) ||
        !result.U8(display) || !result.Utf16Z(sender.c_str(), 72) ||
        !result.Append(ByteView(identity.bytes, sizeof(identity.bytes))))
        return false;
    out = std::move(result);
    return true;
}
bool MapTransferForBuild(unsigned build, uint32_t association, unsigned map, Buffer& out) {
    if (!WorldProtocolBuild(build) || !association || !map || map > 65535)
        return false;
    const uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
    Buffer result;
    if (!result.U16(build == 14075 ? 0x018c : 0x01e3) || !result.Append(ByteView(endpoint, sizeof(endpoint))) ||
        !result.VarU32(0) || !result.VarU32(association) || !result.U16(static_cast<uint16_t>(map)) ||
        !result.VarU32(association) || !result.VarU32(0))
        return false;
    out = std::move(result);
    return true;
}
bool ChatPublication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                     uint8_t display, Buffer& out) {
    return ChatPublicationForBuild(14505, text, sender, identity, channel, display, out);
}
bool MapTransfer(uint32_t association, unsigned map, Buffer& out) {
    return MapTransferForBuild(14505, association, map, out);
}
} // namespace june14505
} // namespace glide
