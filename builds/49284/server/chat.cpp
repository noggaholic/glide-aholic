#include "chat.h"
#include "glide/files.h"
#include <utility>
namespace glide {
namespace hot49284 {
DecodeStatus DecodeChat(ByteView input, ChatMessage& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 154 && id != 155)
        return DecodeStatus::Unknown;
    ChatMessage message;
    message.targeted = id == 154;
    if (message.targeted) {
        const auto status = reader.VarU32(message.target);
        if (status != DecodeStatus::Complete)
            return status;
    }
    std::wstring text;
    for (;;) {
        uint16_t unit;
        if (!reader.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit)
            break;
        if (text.size() == 200)
            return DecodeStatus::Malformed;
        text += static_cast<wchar_t>(unit);
    }
    if (!message.targeted && !reader.U8(message.channel))
        return DecodeStatus::Incomplete;
    if (!WideToUtf8(text, message.text))
        return DecodeStatus::Malformed;
    consumed = reader.Position();
    out = std::move(message);
    return DecodeStatus::Complete;
}
namespace {
bool EncodePublication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                       Buffer& out) {
    if (text.size() > 1004 || text.find('\0') != std::string::npos || sender.size() > 288 ||
        sender.find('\0') != std::string::npos)
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
    if (!result.U16(490) || !result.U16(0x8101) || !result.U16(0x447b) || !result.U16(0x0107))
        return false;
    for (wchar_t unit : wide) {
        if ((unit == L'[' || unit == L']' || unit == L'\\') && !result.U16('\\'))
            return false;
        if (!result.U16(static_cast<uint16_t>(unit)))
            return false;
    }
    if (!result.U16(1) || !result.U16(0) || !result.U16(491) || !result.U8(channel) || !result.U8(17) ||
        !result.Utf16Z(sender.c_str(), 72) || !result.Append(ByteView(identity.bytes, 16)) ||
        !result.VarU32(UINT32_MAX) || !result.VarU32(0))
        return false;
    out = std::move(result);
    return true;
}
} // namespace
bool ServiceChat(const std::string& text, Buffer& out) {
    return EncodePublication(text, "", Guid{}, 15, out);
}
bool IsPublicChat(const ChatMessage& message) {
    return !message.targeted && (message.channel == 25 || message.channel == 26);
}
bool PlayerChat(const ChatMessage& message, const std::string& sender, const Guid& identity, Buffer& out) {
    bool present = false;
    for (uint8_t byte : identity.bytes)
        present = present || byte != 0;
    if (!IsPublicChat(message) || message.flags || message.target || sender.empty() || !present)
        return false;
    return EncodePublication(message.text, sender, identity, message.channel, out);
}
} // namespace hot49284
} // namespace glide
