#include "character_create.h"
#include "glide/profile/character_starting_level.h"
#include "auth_bytes.h"
#include "glide/files.h"
#include <algorithm>
namespace glide {
namespace july14812 {
namespace {
std::string Hex(ByteView bytes) {
    const char* digits = "0123456789abcdef";
    std::string result;
    for (size_t i = 0; i < bytes.size; ++i) {
        result += digits[bytes.data[i] >> 4];
        result += digits[bytes.data[i] & 15];
    }
    return result;
}
bool ValidName(const std::string& name) {
    std::wstring wide;
    if (name.find('\0') != std::string::npos || !Utf8ToWide(name, wide) || wide.empty() || wide.size() > 19)
        return false;
    for (auto unit : wide)
        if (unit < 32 || unit == 127)
            return false;
    return true;
}
} // namespace
DecodeStatus DecodeCreateRequest(ByteView input, CreateRequest& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 147)
        return DecodeStatus::Unknown;
    CreateRequest result;
    auto status = detail::Text(reader, 20, result.name);
    if (status != DecodeStatus::Complete)
        return status;
    if (!detail::Read(reader, result.appearance) || !reader.U8(result.profession) || !reader.U8(result.level))
        return DecodeStatus::Incomplete;
    status = reader.VarU32(result.flags);
    if (status != DecodeStatus::Complete)
        return status;
    uint8_t count;
    if (!reader.U8(count))
        return DecodeStatus::Incomplete;
    if (count > 32)
        return DecodeStatus::Malformed;
    ByteView biography;
    if (!reader.Bytes(count, biography))
        return DecodeStatus::Incomplete;
    if (count)
        result.biography.assign(biography.data, biography.data + count);
    for (auto& slot : result.colors)
        for (auto& color : slot) {
            status = reader.VarU32(color);
            if (status != DecodeStatus::Complete)
                return status;
        }
    consumed = reader.Position();
    out = std::move(result);
    return DecodeStatus::Complete;
}
bool EncodeCreateRequest(const CreateRequest& request, Buffer& out) {
    if (request.name.find('\0') != std::string::npos || request.biography.size() > 32)
        return false;
    Buffer result;
    if (!result.U16(147) || !result.Utf16Z(request.name.c_str(), 19) || !detail::Write(result, request.appearance) ||
        !result.U8(request.profession) || !result.U8(request.level) || !result.VarU32(request.flags) ||
        !result.U8(static_cast<uint8_t>(request.biography.size())) ||
        !result.Append(ByteView(request.biography.data(), request.biography.size())))
        return false;
    for (const auto& slot : request.colors)
        for (auto color : slot)
            if (!result.VarU32(color))
                return false;
    out = std::move(result);
    return true;
}
bool EncodeCreated(const CreatedCharacter& created, Buffer& out) {
    if (created.name.find('\0') != std::string::npos)
        return false;
    Buffer summary, result;
    if (!EncodeSummary(created.summary, summary) || !result.U16(468) || !detail::Write(result, created.character) ||
        !detail::Write(result, created.account) || !result.Utf16Z(created.name.c_str(), 19) ||
        !result.U16(created.retainedWord) || !result.U16(static_cast<uint16_t>(summary.Size())) ||
        !result.Append(summary.View()))
        return false;
    out = std::move(result);
    return true;
}
bool DecodeCreated(ByteView bytes, CreatedCharacter& out) {
    if (!bytes.Valid() || bytes.size > 231)
        return false;
    Reader reader(bytes);
    CreatedCharacter result;
    uint16_t id, length;
    ByteView summary;
    if (!reader.U16(id) || id != 468 || !detail::Read(reader, result.character) ||
        !detail::Read(reader, result.account) || detail::Text(reader, 20, result.name) != DecodeStatus::Complete ||
        !reader.U16(result.retainedWord) || !reader.U16(length) || length != 153 || !reader.Bytes(length, summary) ||
        reader.Remaining() || !DecodeSummary(summary, result.summary))
        return false;
    out = std::move(result);
    return true;
}
DecodeStatus DecodeSubmission(ByteView input, const CharacterCatalog& catalog, Character& out, size_t& consumed) {
    CreateRequest request;
    size_t length = 0;
    const auto status = DecodeCreateRequest(input, request, length);
    if (status != DecodeStatus::Complete)
        return status;
    const auto* recipe =
        catalog.Find(request.appearance[27], request.appearance[29], request.profession, request.level);
    if (!recipe || request.flags || request.appearance[28] > 15 || !ValidName(request.name))
        return DecodeStatus::Malformed;
    Character result;
    result.name = request.name;
    result.level = kMaximumCharacterLevel;
    result.map = recipe->map;
    auto& fields = result.profileFields;
    fields["profession"] = std::to_string(request.profession);
    fields["race"] = std::to_string(request.appearance[27]);
    fields["sex"] = std::to_string(request.appearance[29]);
    fields["body_scale"] = std::to_string(request.appearance[28]);
    fields["appearance"] = Hex(ByteView(request.appearance.data(), 45));
    fields["appearance_tail"] = Hex(ByteView(request.appearance.data() + 45, 3));
    fields["biography"] = Hex(ByteView(request.biography.data(), request.biography.size()));
    fields["creation_flags"] = "0";
    fields["created_from_client"] = "1";
    fields["requested_level"] = std::to_string(request.level);
    fields["effective_level"] = std::to_string(kMaximumCharacterLevel);
    fields["retained_word"] = "0";
    CharacterSummary summary;
    summary.map = recipe->map;
    summary.profession = request.profession;
    summary.level = static_cast<uint8_t>(result.level);
    std::copy(request.appearance.begin(), request.appearance.begin() + 45, summary.appearance.begin());
    for (size_t slot = 0; slot < 6; ++slot) {
        if (slot)
            fields["armor_items"] += ',';
        fields["armor_items"] += std::to_string(recipe->armor[slot].item);
        summary.armor[slot] = recipe->armor[slot].item;
        for (size_t channel = 0; channel < 4; ++channel) {
            const auto raw = request.colors[slot][channel];
            const auto effective = recipe->armor[slot].item ? (raw ? raw : recipe->armor[slot].colorIds[channel]) : 0u;
            uint16_t index = 0;
            if (recipe->armor[slot].item && !DyeIndex(catalog.Palette(), effective, index))
                return DecodeStatus::Malformed;
            summary.colors[slot][channel] = index;
            if (slot || channel) {
                fields["armor_colors"] += ',';
                fields["creation_colors"] += ',';
            }
            fields["armor_colors"] += std::to_string(effective);
            fields["creation_colors"] += std::to_string(raw);
        }
    }
    Buffer encoded;
    if (!EncodeSummary(summary, encoded))
        return DecodeStatus::Malformed;
    result.native.assign(encoded.Data(), encoded.Data() + encoded.Size());
    out = std::move(result);
    consumed = length;
    return DecodeStatus::Complete;
}
bool CreatedReply(const Guid& account, const Character& saved, const CharacterCatalog& catalog, Buffer& out) {
    const auto zero = [](const Guid& guid) {
        return std::all_of(guid.bytes, guid.bytes + 16, [](uint8_t b) { return b == 0; });
    };
    if (zero(account) || zero(saved.guid) || !ValidName(saved.name))
        return false;
    RosterEntry entry;
    std::string error;
    if (!catalog.Resolve(saved, entry, error))
        return false;
    CreatedCharacter result;
    std::copy(account.bytes, account.bytes + 16, result.account.begin());
    result.character = entry.guid;
    result.name = entry.name;
    result.summary = entry.summary;
    const auto word = saved.profileFields.find("retained_word");
    unsigned value = 0;
    if (word != saved.profileFields.end() && !ParseUnsigned(word->second, UINT16_MAX, value))
        return false;
    result.retainedWord = static_cast<uint16_t>(value);
    return EncodeCreated(result, out);
}
} // namespace july14812
} // namespace glide
