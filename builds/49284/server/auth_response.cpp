#include "auth_response.h"
#include "glide/character.h"
#include <algorithm>
#include <utility>
namespace glide {
namespace hot49284 {
namespace {
uint16_t Capability(const AccountSnapshot& account, uint16_t id) {
    for (const auto& cap : account.capabilities)
        if (cap.id == id)
            return cap.value;
    return 0;
}
bool Complete(const AccountSnapshot& account, const std::vector<RosterEntry>& roster, ByteView config, Buffer& out) {
    const unsigned slots = Capability(account, 2) ? 5u + Capability(account, 1) : 2u;
    if (!account.request || !config.Valid() || config.size > 4096 || roster.size() > slots)
        return false;
    Buffer result;
    for (size_t i = 0; i < roster.size(); ++i) {
        const auto& entry = roster[i];
        if (entry.name.empty() || std::all_of(entry.guid.begin(), entry.guid.end(), [](uint8_t b) { return b == 0; }))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (entry.guid == roster[j].guid)
                return false;
        Buffer encoded;
        if (!CharacterEntry(entry, account.request, encoded) || !result.Append(encoded.View()))
            return false;
    }
    if (!result.U16(10) || !result.VarU32(account.request) || !result.U16(static_cast<uint16_t>(config.size)) ||
        !result.Append(config) || !result.U16(4) || !result.VarU32(account.request))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!result.VarU32(0))
            return false;
    out = std::move(result);
    return true;
}
} // namespace
bool CommitConfig(const AuthRequest& request, const std::function<bool(ByteView)>& save, Buffer& out) {
    if (request.operation != AuthOperation::Config || request.id != 12 || !request.request || !request.data.Valid() ||
        request.data.size > 4096 || !save)
        return false;
    Buffer result;
    if (!result.U16(7) || !result.VarU32(request.request))
        return false;
    for (unsigned i = 0; i < 6; ++i)
        if (!result.VarU32(0))
            return false;
    if (!save(request.data))
        return false;
    out = std::move(result);
    return true;
}
bool ReadConfig(const KvDocument& document, Buffer& out) {
    std::string hex;
    std::vector<uint8_t> bytes;
    const auto count = document.Count("", "auth_config");
    if (count && (count != 1 || !document.Get("", "auth_config", hex) || !DecodeHex(hex, 4096, bytes)))
        return false;
    Buffer result;
    if (!result.Append(ByteView(bytes.data(), bytes.size())))
        return false;
    out = std::move(result);
    return true;
}
bool ValidateRedirect(const AuthRequest& request, uint32_t previousRequest) {
    if (request.operation != AuthOperation::Redirect || request.id != 29 || !request.request ||
        request.request == previousRequest || !request.identity.Valid() || request.identity.size != 16 ||
        (request.routing[0] != 1 && request.routing[0] != 3) || request.routing[1] || request.routing[4])
        return false;
    if (request.routing[0] == 1) {
        if (request.routing[2] || request.routing[3])
            return false;
        for (size_t i = 0; i < 16; ++i)
            if (request.identity.data[i])
                return false;
    }
    return true;
}
bool RedirectReply(const AuthRequest& request, uint32_t association, uint32_t map, uint16_t port, Buffer& out) {
    if (!ValidateRedirect(request, 0) || !association || !port ||
        (request.routing[0] == 1 ? map != 0
                                 : (!map || map > UINT16_MAX || (request.routing[2] && request.routing[2] != map))))
        return false;
    uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
    endpoint[2] = static_cast<uint8_t>(port >> 8);
    endpoint[3] = static_cast<uint8_t>(port);
    Buffer reply;
    if (!reply.U16(20) || !reply.VarU32(request.request) || !reply.VarU32(request.routing[0]) ||
        !reply.VarU32(association) || !reply.VarU32(map) || !reply.Append(ByteView(endpoint, sizeof(endpoint))) ||
        !reply.VarU32(request.routing[3]))
        return false; // Own redirect context, not June's generated nonce.
    out = std::move(reply);
    return true;
}
bool CharacterEntry(const RosterEntry& entry, uint32_t request, Buffer& out) {
    if (entry.name.find('\0') != std::string::npos)
        return false;
    Buffer summary, result;
    if (!EncodeSummary(entry.summary, summary) || !result.U16(17) || !result.VarU32(request) ||
        !result.Append(ByteView(entry.guid.data(), entry.guid.size())) || !result.VarU32(entry.retainedScalar) ||
        !result.U64(entry.retainedValues[0]) || !result.U64(entry.retainedValues[1]) ||
        !result.Utf16Z(entry.name.c_str(), 19) || !result.U16(static_cast<uint16_t>(summary.Size())) ||
        !result.Append(summary.View()))
        return false;
    out = std::move(result);
    return true;
}
bool LoginReplies::Begin(const AccountSnapshot& account, const std::vector<RosterEntry>& roster, ByteView config,
                         std::vector<Buffer>& out) {
    if (pending_)
        return false;
    Buffer state, completion;
    if (!EncodeAccount(account, state) || !Complete(account, roster, config, completion))
        return false;
    std::vector<Buffer> result;
    result.push_back(std::move(state));
    if (contentReported_)
        result.push_back(std::move(completion));
    else {
        completion_ = std::move(completion);
        pending_ = true;
    }
    out = std::move(result);
    return true;
}
bool LoginReplies::ContentReported(std::vector<Buffer>& out) {
    if (contentReported_)
        return false;
    std::vector<Buffer> result;
    if (pending_)
        result.push_back(std::move(completion_));
    pending_ = false;
    contentReported_ = true;
    out = std::move(result);
    return true;
}
} // namespace hot49284
} // namespace glide
