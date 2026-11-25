#include "auth_response.h"
#include "auth_bytes.h"
#include "glide/character.h"
#include <algorithm>
#include <utility>
namespace glide {
namespace july14812 {
namespace {
bool Zero(const std::array<uint8_t, 16>& value) {
    return std::all_of(value.begin(), value.end(), [](uint8_t b) { return b == 0; });
}
} // namespace
bool CharacterEntry(const RosterEntry& entry, uint32_t request, Buffer& out) {
    if (entry.name.find('\0') != std::string::npos)
        return false;
    Buffer summary, result;
    if (!EncodeSummary(entry.summary, summary) || !result.U16(17) || !result.VarU32(request) ||
        !detail::Write(result, entry.guid) || !result.VarU32(entry.retainedScalar) ||
        !result.Utf16Z(entry.name.c_str(), 19) || !result.U16(static_cast<uint16_t>(summary.Size())) ||
        !result.Append(summary.View()))
        return false;
    out = std::move(result);
    return true;
}
bool DecodeCharacterEntry(ByteView bytes, RosterEntry& out) {
    if (!bytes.Valid() || bytes.size > 223)
        return false;
    Reader reader(bytes);
    uint16_t id, length;
    RosterEntry result;
    ByteView summary;
    if (!reader.U16(id) || id != 17 || !detail::Var(reader, result.request) || !detail::Read(reader, result.guid) ||
        !detail::Var(reader, result.retainedScalar) ||
        detail::Text(reader, 20, result.name) != DecodeStatus::Complete || !reader.U16(length) || length != 153 ||
        !reader.Bytes(length, summary) || reader.Remaining() || !DecodeSummary(summary, result.summary))
        return false;
    out = std::move(result);
    return true;
}
bool Completion(uint32_t request, Buffer& out) {
    if (!request)
        return false;
    Buffer result;
    if (!result.U16(4) || !result.VarU32(request))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!result.VarU32(0))
            return false;
    out = std::move(result);
    return true;
}
bool ReadConfig(const KvDocument& document, Buffer& out) {
    const auto count = document.Count("", "auth_config");
    std::string hex;
    std::vector<uint8_t> bytes;
    if (count && (count != 1 || !document.Get("", "auth_config", hex) || !DecodeHex(hex, 4096, bytes)))
        return false;
    Buffer result;
    if (!result.Append(ByteView(bytes.data(), bytes.size())))
        return false;
    out = std::move(result);
    return true;
}
bool LoginReplies::Begin(const AccountSnapshot& account, const std::vector<RosterEntry>& roster, ByteView config,
                         std::vector<Buffer>& out, const std::function<bool()>& admit) {
    if (pending_ || !account.request || Zero(account.user) || !ValidAccountType(account.type) || !config.Valid() ||
        config.size > 4096 || roster.size() > CharacterSlots(account))
        return false;
    Buffer state, complete, ack;
    if (!EncodeAccount(account, state))
        return false;
    bool selected = roster.empty() && Zero(account.selected);
    for (size_t i = 0; i < roster.size(); ++i) {
        const auto& entry = roster[i];
        if (entry.name.empty() || Zero(entry.guid))
            return false;
        for (size_t j = 0; j < i; ++j)
            if (entry.guid == roster[j].guid || entry.name == roster[j].name)
                return false;
        if (entry.guid == account.selected)
            selected = true;
        Buffer encoded;
        if (!CharacterEntry(entry, account.request, encoded) || !complete.Append(encoded.View()))
            return false;
    }
    if (!selected || !complete.U16(10) || !complete.VarU32(account.request) ||
        !complete.U16(static_cast<uint16_t>(config.size)) || !complete.Append(config) ||
        !Completion(account.request, ack) || !complete.Append(ack.View()))
        return false;
    std::vector<Buffer> result;
    result.push_back(std::move(state));
    if (admit && !admit())
        return false;
    if (reported_)
        result.push_back(std::move(complete));
    else {
        completion_ = std::move(complete);
        pending_ = true;
    }
    out = std::move(result);
    return true;
}
bool LoginReplies::ContentReported(std::vector<Buffer>& out) {
    std::vector<Buffer> result;
    if (reported_) {
        out = std::move(result);
        return true;
    }
    if (!pending_)
        return false;
    result.push_back(std::move(completion_));
    pending_ = false;
    reported_ = true;
    out = std::move(result);
    return true;
}
bool ConfigUpload::Begin(uint32_t request, uint32_t total) {
    if (active_ || !request || total > 4096)
        return false;
    bytes_.Clear();
    request_ = request;
    total_ = total;
    active_ = true;
    return true;
}
bool ConfigUpload::Chunk(uint32_t request, ByteView bytes, const std::function<bool(ByteView)>& save, Buffer& out) {
    if (!active_ || request != request_ || !bytes.Valid() || bytes.size > 512 || bytes_.Size() > total_)
        return false;
    const size_t remaining = total_ - bytes_.Size();
    if (bytes.size > remaining || (!bytes.size && remaining))
        return false;
    if (bytes.size < remaining) {
        if (!bytes_.Append(bytes))
            return false;
        Buffer empty;
        out = std::move(empty);
        return true;
    }
    Buffer complete, ack;
    if (!save || !complete.Append(bytes_.View()) || !complete.Append(bytes) || !ack.U16(7) || !ack.VarU32(request_))
        return false;
    for (unsigned i = 0; i < 6; ++i)
        if (!ack.VarU32(0))
            return false;
    if (!save(complete.View()))
        return false;
    bytes_.Clear();
    active_ = false;
    request_ = 0;
    total_ = 0;
    out = std::move(ack);
    return true;
}
} // namespace july14812
} // namespace glide
