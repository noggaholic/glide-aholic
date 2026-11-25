#include "glide/character.h"
#include "glide/files.h"
#include <Windows.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <locale.h>
#include <set>

namespace glide {
namespace {
bool Number(const KvDocument& doc, const std::string& section, const char* key, unsigned maximum, unsigned& out,
            bool required) {
    if (!doc.Count(section, key))
        return !required;
    std::string text;
    return doc.Get(section, key, text) && ParseUnsigned(text, maximum, out);
}
bool ParsePosition(const std::string& text, float& out) {
    _locale_t locale = _create_locale(LC_NUMERIC, "C");
    if (!locale)
        return false;
    char* end = nullptr;
    errno = 0;
    const float value = _strtof_l(text.c_str(), &end, locale);
    const bool valid = end != text.c_str() && errno != ERANGE && std::isfinite(value);
    _free_locale(locale);
    if (!valid)
        return false;
    while (*end == ' ' || *end == '\t')
        ++end;
    if (*end)
        return false;
    out = value;
    return true;
}
bool Position(const KvDocument& doc, const std::string& section, const char* key, float& out) {
    std::string text;
    return doc.Get(section, key, text) && ParsePosition(text, out);
}
} // namespace
bool ValidMarkerName(const std::string& name) {
    if (name.empty() || name.size() > 63 || name.front() == ' ' || name.back() == ' ')
        return false;
    std::wstring wide;
    if (!Utf8ToWide(name, wide))
        return false;
    for (wchar_t c : wide)
        if (c < 32 || c == 127 || c == '|')
            return false;
    return true;
}
bool MarkerNameEqual(const std::string& a, const std::string& b) {
    std::wstring first, second;
    return Utf8ToWide(a, first) && Utf8ToWide(b, second) &&
           CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
}
bool DecodeHex(const std::string& text, size_t maxBytes, std::vector<uint8_t>& out) {
    if ((text.size() & 1) || text.size() / 2 > maxBytes)
        return false;
    std::vector<uint8_t> bytes;
    unsigned high = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        unsigned digit;
        if (c >= '0' && c <= '9')
            digit = static_cast<unsigned>(c - '0');
        else if (c >= 'a' && c <= 'f')
            digit = static_cast<unsigned>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            digit = static_cast<unsigned>(c - 'A' + 10);
        else
            return false;
        if ((i & 1) == 0)
            high = digit;
        else
            bytes.push_back(static_cast<uint8_t>((high << 4) | digit));
    }
    out = std::move(bytes);
    return true;
}
bool DecodeAccountState(const KvDocument& doc, AccountState& out, std::string& error) {
    AccountState state;
    std::string guid;
    error = "Invalid account state";
    if (doc.Count("", "completed_hints")) {
        std::string text;
        std::vector<uint8_t> bytes;
        if (doc.Count("", "completed_hints") != 1 || !doc.Get("", "completed_hints", text) ||
            !DecodeHex(text, 32, bytes) || bytes.size() != 32)
            return false;
        for (size_t i = 0; i < bytes.size(); ++i)
            state.completedHints[i / 4] |= static_cast<uint32_t>(bytes[i]) << ((i % 4) * 8);
    }
    if (!doc.Get("", "user_guid", guid) || !Guid::Parse(guid, state.userGuid) ||
        !Number(doc, "", "coins", UINT32_MAX, state.coins, true) ||
        !Number(doc, "", "karma", UINT32_MAX, state.karma, true) ||
        !Number(doc, "", "home_world", UINT32_MAX, state.homeWorld, true) ||
        !Number(doc, "", "access_level", UINT32_MAX, state.accessLevel, true) ||
        !Number(doc, "", "world_class", UINT32_MAX, state.worldClass, false) ||
        !Number(doc, "", "instance_type", UINT32_MAX, state.instanceType, false) ||
        !Number(doc, "", "selected", 4, state.selected, true))
        return false;
    for (const auto& field : doc.Entries("")) {
        if (field.first != "marker")
            continue;
        error = "Invalid saved marker";
        std::vector<std::string> parts;
        size_t start = 0;
        for (;;) {
            const auto end = field.second.find('|', start);
            std::string part = field.second.substr(start, end == std::string::npos ? end : end - start);
            const auto first = part.find_first_not_of(" \t");
            part = first == std::string::npos ? std::string()
                                              : part.substr(first, part.find_last_not_of(" \t") - first + 1);
            parts.push_back(std::move(part));
            if (end == std::string::npos || parts.size() > 5)
                break;
            start = end + 1;
        }
        Marker marker;
        if (parts.size() != 5 || !ValidMarkerName(parts[0]) || !ParseUnsigned(parts[1], 65535, marker.map) ||
            !marker.map || !ParsePosition(parts[2], marker.x) || !ParsePosition(parts[3], marker.y) ||
            !ParsePosition(parts[4], marker.z) || !std::isfinite(marker.x * 32) || !std::isfinite(marker.y * 32) ||
            !std::isfinite(marker.z * 32))
            return false;
        marker.name = parts[0];
        for (const auto& existing : state.markers)
            if (MarkerNameEqual(existing.name, marker.name))
                return false;
        state.markers.push_back(std::move(marker));
    }
    std::set<unsigned> slots;
    std::set<std::string> guids;
    std::vector<std::wstring> names;
    for (const auto& section : doc.Sections()) {
        if (section.compare(0, 10, "character ") != 0)
            continue;
        Character character;
        error = "Invalid " + section;
        if (!ParseUnsigned(section.substr(10), 4, character.slot) ||
            section != "character " + std::to_string(character.slot) || !slots.insert(character.slot).second ||
            !doc.Get(section, "guid", guid) || !Guid::Parse(guid, character.guid) ||
            !guids.insert(character.guid.Text()).second || !doc.Get(section, "name", character.name) ||
            !Number(doc, section, "level", 255, character.level, true) || !character.level ||
            !Number(doc, section, "map", 65535, character.map, true) || !character.map ||
            !Number(doc, section, "glider", UINT32_MAX, character.glider, false) ||
            !Number(doc, section, "mount", UINT32_MAX, character.mount, false) ||
            !Position(doc, section, "x", character.x) || !Position(doc, section, "y", character.y) ||
            !Position(doc, section, "z", character.z))
            return false;
        std::wstring name;
        if (!Utf8ToWide(character.name, name) || name.empty() || name.size() > 20)
            return false;
        for (wchar_t c : name)
            if (c < 32 || c == 127)
                return false;
        for (const auto& other : names)
            if (CompareStringOrdinal(name.c_str(), -1, other.c_str(), -1, TRUE) == CSTR_EQUAL)
                return false;
        names.push_back(name);
        for (const auto& field : doc.Entries(section)) {
            // Reject ambiguous fields in a character, but retain all original text for later saves.
            if (!character.profileFields.emplace(field.first, field.second).second)
                return false;
        }
        const auto native = character.profileFields.find("native");
        if (native != character.profileFields.end() && !DecodeHex(native->second, 65535, character.native))
            return false;
        state.characters.push_back(std::move(character));
    }
    if ((!state.characters.empty() && !slots.count(state.selected)) ||
        (state.characters.empty() && state.selected != 0))
        return false;
    std::sort(state.characters.begin(), state.characters.end(),
              [](const Character& a, const Character& b) { return a.slot < b.slot; });
    state.document = doc;
    out = std::move(state);
    error.clear();
    return true;
}
} // namespace glide
