#include "character_create.h"
#include "glide/profile/character_starting_level.h"
#include "glide/files.h"
#include <algorithm>
namespace glide {
namespace hot49284 {
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
} // namespace
DecodeStatus DecodeSubmission(ByteView input, const CharacterCatalog& catalog, Character& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    if (id != 251)
        return DecodeStatus::Unknown;
    std::wstring name;
    bool terminated = false;
    for (unsigned i = 0; i < 20; ++i) {
        uint16_t unit;
        if (!reader.U16(unit))
            return DecodeStatus::Incomplete;
        if (!unit) {
            terminated = true;
            break;
        }
        if (unit < 32 || unit == 127)
            return DecodeStatus::Malformed;
        name += static_cast<wchar_t>(unit);
    }
    Character result;
    if (!terminated || name.empty() || !WideToUtf8(name, result.name))
        return DecodeStatus::Malformed;
    ByteView appearance, biography;
    uint8_t profession, level, count;
    uint32_t flags;
    if (!reader.Bytes(48, appearance) || !reader.U8(profession) || !reader.U8(level))
        return DecodeStatus::Incomplete;
    auto status = reader.VarU32(flags);
    if (status != DecodeStatus::Complete)
        return status;
    if (!reader.U8(count))
        return DecodeStatus::Incomplete;
    if (count > 32)
        return DecodeStatus::Malformed;
    if (!reader.Bytes(count, biography))
        return DecodeStatus::Incomplete;
    std::array<uint32_t, 24> colors{};
    for (auto& color : colors) {
        status = reader.VarU32(color);
        if (status != DecodeStatus::Complete)
            return status;
    }
    const auto* recipe = catalog.Find(appearance.data[27], appearance.data[29], profession, level);
    if (!recipe || flags || appearance.data[28] > 15)
        return DecodeStatus::Malformed;
    result.level = kMaximumCharacterLevel;
    result.map = recipe->map;
    auto& fields = result.profileFields;
    fields["profession"] = std::to_string(profession);
    fields["race"] = std::to_string(appearance.data[27]);
    fields["sex"] = std::to_string(appearance.data[29]);
    fields["body_scale"] = std::to_string(appearance.data[28]);
    fields["appearance"] = Hex(ByteView(appearance.data, 45));
    fields["appearance_tail"] = Hex(ByteView(appearance.data + 45, 3));
    fields["biography"] = Hex(biography);
    fields["creation_flags"] = "0";
    fields["created_from_client"] = "1";
    fields["requested_level"] = std::to_string(level);
    fields["effective_level"] = std::to_string(kMaximumCharacterLevel);
    for (size_t slot = 0; slot < 6; ++slot) {
        if (slot)
            fields["armor_items"] += ',';
        fields["armor_items"] += std::to_string(recipe->armor[slot]);
        for (size_t channel = 0; channel < 4; ++channel) {
            const auto requested = colors[slot * 4 + channel];
            const auto color = recipe->armor[slot] ? (requested ? requested : recipe->colors[slot][channel]) : 0u;
            if (recipe->armor[slot] && !catalog.HasColor(color))
                return DecodeStatus::Malformed;
            if (slot || channel)
                fields["armor_colors"] += ',';
            fields["armor_colors"] += std::to_string(color);
        }
    }
    RosterEntry checked;
    std::string error;
    if (!catalog.Resolve(result, checked, error))
        return DecodeStatus::Malformed;
    out = std::move(result);
    consumed = reader.Position();
    return DecodeStatus::Complete;
}
bool CreatedReply(const Guid& account, const Character& character, const CharacterCatalog& catalog, Buffer& out) {
    const auto zero = [](const Guid& guid) {
        return std::all_of(guid.bytes, guid.bytes + 16, [](uint8_t b) { return b == 0; });
    };
    if (zero(account) || zero(character.guid) || character.name.empty())
        return false;
    RosterEntry entry;
    std::string error;
    Buffer summary, reply;
    if (!catalog.Resolve(character, entry, error) || !EncodeSummary(entry.summary, summary) || !reply.U16(737) ||
        !reply.Append(ByteView(character.guid.bytes, 16)) || !reply.Append(ByteView(account.bytes, 16)) ||
        !reply.Utf16Z(character.name.c_str(), 19) || !reply.U16(0) ||
        !reply.U16(static_cast<uint16_t>(summary.Size())) || !reply.Append(summary.View()))
        return false;
    out = std::move(reply);
    return true;
}
} // namespace hot49284
} // namespace glide
