#include "auth_response.h"
#include <utility>

namespace glide {
namespace june14505 {
bool CharacterSummary(const RosterCharacter& character, Buffer& out) {
    if (character.map > UINT16_MAX || !character.level || character.level > 80 || !character.profession ||
        character.profession > 8 || character.appearance[27] >= 5 || character.appearance[28] >= 16 ||
        character.appearance[29] >= 3)
        return false;
    Buffer result;
    if (!result.U8(7) || !result.U16(static_cast<uint16_t>(character.map)) || !result.U8(0) ||
        !result.U8(character.level) || !result.U8(character.profession) ||
        !result.Append(ByteView(character.appearance.data(), character.appearance.size())) || !result.Resize(61))
        return false; // Empty hands and equipment flags.
    for (size_t slot = 0; slot < character.armor.size(); ++slot) {
        if (!result.U16(character.armor[slot]) || !result.U16(0))
            return false;
        for (uint16_t dye : character.dyes[slot])
            if (!result.U16(character.armor[slot] ? dye : 0))
                return false;
    }
    // Seventh armor slot and first trailing float are unused in the retained format.
    if (!result.Resize(149) || !result.U32(0xe8672c39u))
        return false;
    out = std::move(result);
    return true;
}
bool CharacterEntry(const RosterCharacter& character, uint32_t request, Buffer& out) {
    Buffer summary, result;
    if (!request || character.name.empty() || character.name.find('\0') != std::string::npos ||
        !CharacterSummary(character, summary) || !result.U16(17) || !result.VarU32(request) ||
        !result.Append(ByteView(character.guid.bytes, 16)) || !result.VarU32(0) ||
        !result.Utf16Z(character.name.c_str(), 19) || !result.U16(static_cast<uint16_t>(summary.Size())) ||
        !result.Append(summary.View()))
        return false;
    out = std::move(result);
    return true;
}
bool AccountReply(const AccountState& account, const Guid& selected, uint32_t request, ByteView contentSeed,
                  Buffer& out) {
    if (!request || !contentSeed.Valid() || contentSeed.size != 32)
        return false;
    unsigned agreement = 0;
    if (account.document.Count("", "agreement_version")) {
        std::string value;
        if (account.document.Count("", "agreement_version") != 1 ||
            !account.document.Get("", "agreement_version", value) || !ParseUnsigned(value, 255, agreement))
            return false;
    }
    const uint8_t zero[32] = {};
    Buffer result;
    if (!result.U16(8) || !result.VarU32(request) || !result.Append(ByteView(account.userGuid.bytes, 16)) ||
        !result.Append(ByteView(selected.bytes, 16)) || !result.VarU32(account.accessLevel) ||
        !result.VarU32(agreement) || !result.VarU32(account.homeWorld) || !result.VarU32(account.worldClass) ||
        !result.Append(contentSeed) || !result.U8(0) || !result.Append(ByteView(zero, sizeof(zero))))
        return false;
    out = std::move(result);
    return true;
}
bool LoginComplete(const std::vector<RosterCharacter>& characters, uint32_t request, std::vector<Buffer>& out,
                   ByteView preferences) {
    if (!request || !preferences.Valid() || preferences.size > 1024)
        return false;
    std::vector<Buffer> result;
    for (const auto& character : characters) {
        Buffer entry;
        if (!CharacterEntry(character, request, entry))
            return false;
        result.push_back(std::move(entry));
    }
    Buffer config, complete;
    if (!config.U16(10) || !config.VarU32(request) || !config.U16(static_cast<uint16_t>(preferences.size)) ||
        !config.Append(preferences) || !complete.U16(4) || !complete.VarU32(request))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!complete.VarU32(0))
            return false;
    result.push_back(std::move(config));
    result.push_back(std::move(complete));
    out = std::move(result);
    return true;
}
bool LoginReplies::Begin(const AccountState& account, const std::vector<RosterCharacter>& characters, uint32_t request,
                         ByteView contentSeed, std::vector<Buffer>& out, ByteView config) {
    if (pending_)
        return false;
    Buffer state;
    std::vector<Buffer> completion;
    // Initial June login uses the account GUID in both identity fields.
    if (!AccountReply(account, account.userGuid, request, contentSeed, state) ||
        !LoginComplete(characters, request, completion, config))
        return false;
    return BeginPrepared(std::move(state), std::move(completion), out);
}
bool LoginReplies::BeginPrepared(Buffer state, std::vector<Buffer> completion, std::vector<Buffer>& out) {
    if (pending_ || state.Size() < 2 || completion.empty())
        return false;
    for (const auto& frame : completion)
        if (frame.Size() < 2)
            return false;
    std::vector<Buffer> result;
    result.push_back(std::move(state));
    if (contentReported_) {
        for (auto& frame : completion)
            result.push_back(std::move(frame));
    } else {
        completion_ = std::move(completion);
        pending_ = true;
    }
    out = std::move(result);
    return true;
}
bool LoginReplies::ContentReported(std::vector<Buffer>& out) {
    if (contentReported_)
        return false;
    contentReported_ = true;
    std::vector<Buffer> result;
    if (pending_) {
        result = std::move(completion_);
        pending_ = false;
    }
    out = std::move(result);
    return true;
}
bool ReadConfig(const KvDocument& document, Buffer& out) {
    std::string hex;
    std::vector<uint8_t> bytes;
    if (document.Count("", "auth_config") && (!document.Get("", "auth_config", hex) || !DecodeHex(hex, 1024, bytes)))
        return false;
    Buffer result;
    if (!result.Append(ByteView(bytes.data(), bytes.size())))
        return false;
    out = std::move(result);
    return true;
}
bool SelectionReply(uint32_t request, uint32_t map, Buffer& out) {
    if (!request || !map || map > 65535)
        return false;
    const uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
    Buffer result;
    if (!result.U16(3) || !result.VarU32(request) || !result.VarU32(map) ||
        !result.Append(ByteView(endpoint, sizeof(endpoint))))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!result.VarU32(0))
            return false;
    out = std::move(result);
    return true;
}
bool ConfigUpload::Begin(uint32_t request, uint32_t total) {
    if (active_ || !request || total > 1024)
        return false;
    bytes_.Clear();
    request_ = request;
    total_ = total;
    active_ = true;
    receivedChunk_ = false;
    return true;
}
bool ConfigUpload::Chunk(uint32_t request, ByteView bytes) {
    if (!active_ || Ready() || request != request_ || !bytes.Valid() || bytes.size > 512 ||
        bytes.size > total_ - bytes_.Size())
        return false;
    if (!bytes_.Append(bytes))
        return false;
    receivedChunk_ = true;
    return true;
}
bool ConfigUpload::Finish(const std::function<bool(ByteView)>& save, Buffer& out) {
    if (!Ready() || !save)
        return false;
    Buffer result;
    if (!result.U16(7) || !result.VarU32(request_))
        return false;
    for (unsigned i = 0; i < 6; ++i)
        if (!result.VarU32(0))
            return false;
    if (!save(bytes_.View()))
        return false;
    active_ = false;
    receivedChunk_ = false;
    bytes_.Clear();
    out = std::move(result);
    return true;
}
} // namespace june14505
} // namespace glide
