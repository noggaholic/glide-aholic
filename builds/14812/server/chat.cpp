#include "chat.h"
#include "auth_bytes.h"
#include "glide/files.h"
namespace glide {
namespace july14812 {
DecodeStatus DecodeChat(ByteView input, ChatMessage& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 83 && id != 84)
        return DecodeStatus::Unknown;
    ChatMessage result;
    result.targeted = id == 83;
    if (result.targeted) {
        const auto status = reader.VarU32(result.target);
        if (status != DecodeStatus::Complete)
            return status;
    }
    const auto status = detail::Text(reader, 201, result.text);
    if (status != DecodeStatus::Complete)
        return status;
    if (!result.targeted && !reader.U8(result.channel))
        return DecodeStatus::Incomplete;
    consumed = reader.Position();
    out = std::move(result);
    return DecodeStatus::Complete;
}
namespace {
bool Publication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                 Buffer& out) {
    std::wstring wide, name;
    if (text.size() > 1004 || sender.size() > 288 || text.find('\0') != std::string::npos ||
        sender.find('\0') != std::string::npos || !Utf8ToWide(text, wide) || wide.size() > 251 ||
        !Utf8ToWide(sender, name) || name.size() > 72)
        return false;
    size_t units = 5;
    for (wchar_t unit : wide)
        units += unit == L'[' || unit == L']' || unit == L'\\' ? 2 : 1;
    if (units > 256)
        return false;
    Buffer result;
    if (!result.U16(307) || !result.U16(0x8101) || !result.U16(0x447b) || !result.U16(0x0107))
        return false;
    for (wchar_t unit : wide) {
        if ((unit == L'[' || unit == L']' || unit == L'\\') && !result.U16('\\'))
            return false;
        if (!result.U16(static_cast<uint16_t>(unit)))
            return false;
    }
    if (!result.U16(1) || !result.U16(0) || !result.U16(308) || !result.U8(channel) || !result.U8(17) ||
        !result.Utf16Z(sender.c_str(), 72) || !result.Append(ByteView(identity.bytes, sizeof(identity.bytes))))
        return false;
    out = std::move(result);
    return true;
}
} // namespace
bool IsPublicChat(const ChatMessage& message) {
    return !message.targeted && !message.flags && (message.channel == 8 || message.channel == 9);
}
bool PublishChat(const ChatMessage& message, const std::string& sender, const Guid& identity, Buffer& out) {
    return IsPublicChat(message) && !sender.empty() &&
           Publication(message.text, sender, identity, message.channel, out);
}
bool ServiceChat(const std::string& text, Buffer& out) {
    return Publication(text, "", Guid{}, 9, out);
}
} // namespace july14812
} // namespace glide
