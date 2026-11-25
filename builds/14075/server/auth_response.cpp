#include "auth_response.h"

namespace glide {
namespace april14075 {
namespace {
bool SetId(Buffer& message, uint16_t id) {
    if (message.Size() < 2)
        return false;
    message.Data()[0] = static_cast<uint8_t>(id);
    message.Data()[1] = static_cast<uint8_t>(id >> 8);
    return true;
}
} // namespace
bool AccountReply(const AccountState& account, const Guid& selected, uint32_t request, ByteView contentSeed,
                  Buffer& out) {
    if (!request || !contentSeed.Valid() || contentSeed.size != 32)
        return false;
    const uint8_t zero[32] = {};
    Buffer result;
    if (!result.U16(17) || !result.VarU32(request) || !result.Append(ByteView(account.userGuid.bytes, 16)) ||
        !result.Append(ByteView(selected.bytes, 16)) || !result.VarU32(account.homeWorld) ||
        !result.VarU32(account.worldClass) || !result.VarU32(account.accessLevel) || !result.VarU32(0) ||
        !result.Append(contentSeed) || !result.U8(0) || !result.Append(ByteView(zero, 32)))
        return false;
    out = std::move(result);
    return true;
}
bool CharacterEntry(const RosterCharacter& character, uint32_t request, Buffer& out) {
    Buffer result;
    if (!june14505::CharacterEntry(character, request, result) || !SetId(result, 7))
        return false;
    out = std::move(result);
    return true;
}
bool LoginComplete(const std::vector<RosterCharacter>& characters, uint32_t request, std::vector<Buffer>& out,
                   ByteView config) {
    std::vector<Buffer> result;
    if (!june14505::LoginComplete(characters, request, result, config))
        return false;
    for (size_t i = 0; i < result.size(); ++i) {
        const uint16_t id = i < characters.size() ? 7 : i == characters.size() ? 22 : 3;
        if (!SetId(result[i], id))
            return false;
    }
    out = std::move(result);
    return true;
}
bool SelectionReply(uint32_t request, uint32_t map, Buffer& out) {
    Buffer result;
    if (!june14505::SelectionReply(request, map, result) || !SetId(result, 8))
        return false;
    out = std::move(result);
    return true;
}
} // namespace april14075
} // namespace glide
