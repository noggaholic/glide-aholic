#include "glide/kv.h"
#include <Windows.h>
#include <climits>
#include <algorithm>
#include <utility>

namespace glide {
namespace {
std::string Trim(const std::string& text) {
    const size_t start = text.find_first_not_of(" \t");
    if (start == std::string::npos)
        return {};
    return text.substr(start, text.find_last_not_of(" \t") - start + 1);
}
bool SingleLine(const std::string& text) {
    return text.find_first_of("\r\n") == std::string::npos && text.find('\0') == std::string::npos;
}
bool KeyValid(const std::string& key) {
    if (key.empty())
        return false;
    for (char c : key)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '.'))
            return false;
    return true;
}
} // namespace
bool KvDocument::Parse(const std::string& text, KvDocument& out, std::string& error) {
    if (text.size() > INT_MAX || text.find('\0') != std::string::npos ||
        (!text.empty() && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                              nullptr, 0) <= 0)) {
        error = "Expected UTF-8 text without null bytes";
        return false;
    }
    KvDocument document;
    std::string section;
    size_t at = text.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    unsigned lineNumber = 0;
    while (at < text.size()) {
        ++lineNumber;
        size_t next = text.find('\n', at);
        if (next == std::string::npos)
            next = text.size();
        Line line;
        line.raw = text.substr(at, next - at);
        if (!line.raw.empty() && line.raw.back() == '\r')
            line.raw.pop_back();
        const std::string trimmed = Trim(line.raw);
        if (!SingleLine(line.raw)) {
            error = "Invalid line " + std::to_string(lineNumber);
            return false;
        }
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';') {
            line.section = section;
        } else if (trimmed[0] == '[') {
            if (trimmed.back() != ']' || trimmed.size() < 3) {
                error = "Invalid section on line " + std::to_string(lineNumber);
                return false;
            }
            if (trimmed == "[end]") {
                if (section.empty()) {
                    error = "Unexpected [end]";
                    return false;
                }
                section.clear();
            } else {
                if (!section.empty()) {
                    error = "Close the current section with [end]";
                    return false;
                }
                section = trimmed.substr(1, trimmed.size() - 2);
                if (section.find_first_of("[]") != std::string::npos) {
                    error = "Invalid section name";
                    return false;
                }
            }
            line.section = section;
        } else {
            const size_t equals = line.raw.find('=');
            if (equals == std::string::npos || !KeyValid(Trim(line.raw.substr(0, equals)))) {
                error = "Expected key=value on line " + std::to_string(lineNumber);
                return false;
            }
            line.section = section;
            line.key = Trim(line.raw.substr(0, equals));
            line.value = line.raw.substr(equals + 1);
            line.entry = true;
        }
        document.lines_.push_back(std::move(line));
        at = next + 1;
    }
    if (!section.empty()) {
        error = "Missing [end]";
        return false;
    }
    out = std::move(document);
    error.clear();
    return true;
}
size_t KvDocument::Count(const std::string& section, const std::string& key) const {
    size_t count = 0;
    for (const auto& line : lines_)
        if (line.entry && line.section == section && line.key == key)
            ++count;
    return count;
}
bool KvDocument::Get(const std::string& section, const std::string& key, std::string& value) const {
    const Line* found = nullptr;
    for (const auto& line : lines_) {
        if (!line.entry || line.section != section || line.key != key)
            continue;
        if (found)
            return false;
        found = &line;
    }
    if (!found)
        return false;
    value = found->value;
    return true;
}
bool KvDocument::Set(const std::string& section, const std::string& key, const std::string& value) {
    if (!KeyValid(key) || !SingleLine(value) || Count(section, key) > 1)
        return false;
    for (auto& line : lines_) {
        if (line.entry && line.section == section && line.key == key) {
            line.value = value;
            line.raw = key + "=" + value;
            return true;
        }
    }
    Line line;
    line.section = section;
    line.key = key;
    line.value = value;
    line.raw = key + "=" + value;
    line.entry = true;
    if (section.empty()) {
        // Put new account keys before any character section.
        lines_.insert(lines_.begin(), std::move(line));
        return true;
    }
    for (auto it = lines_.begin(); it != lines_.end(); ++it) {
        if (!it->entry && Trim(it->raw) == "[" + section + "]") {
            lines_.insert(it + 1, std::move(line));
            return true;
        }
    }
    Line begin, end;
    begin.section = section;
    begin.raw = "[" + section + "]";
    end.raw = "[end]";
    if (section.find_first_of("[]\r\n") != std::string::npos || !SingleLine(section) || section == "end")
        return false;
    lines_.push_back(std::move(begin));
    lines_.push_back(std::move(line));
    lines_.push_back(std::move(end));
    return true;
}
bool KvDocument::SetValues(const std::string& section, const std::string& key, const std::vector<std::string>& values) {
    if (!KeyValid(key) || !SingleLine(section) || section == "end" || section.find_first_of("[]") != std::string::npos)
        return false;
    for (const auto& value : values)
        if (!SingleLine(value))
            return false;
    KvDocument updated = *this;
    updated.lines_.erase(
        std::remove_if(updated.lines_.begin(), updated.lines_.end(),
                       [&](const Line& line) { return line.entry && line.section == section && line.key == key; }),
        updated.lines_.end());
    if (!values.empty()) {
        if (!updated.Set(section, key, values[0]))
            return false;
        for (auto at = updated.lines_.begin(); at != updated.lines_.end(); ++at) {
            if (!at->entry || at->section != section || at->key != key)
                continue;
            std::vector<Line> extra;
            for (size_t i = 1; i < values.size(); ++i) {
                Line line;
                line.entry = true;
                line.section = section;
                line.key = key;
                line.value = values[i];
                line.raw = key + "=" + values[i];
                extra.push_back(std::move(line));
            }
            updated.lines_.insert(at + 1, extra.begin(), extra.end());
            break;
        }
    }
    *this = std::move(updated);
    return true;
}
bool KvDocument::RemoveSection(const std::string& section) {
    if (section.empty() || section == "end" || !SingleLine(section) || section.find_first_of("[]") != std::string::npos)
        return false;
    bool found = false;
    for (auto at = lines_.begin(); at != lines_.end();) {
        if (at->entry || Trim(at->raw) != "[" + section + "]") {
            ++at;
            continue;
        }
        auto end = at + 1;
        while (end != lines_.end() && (end->entry || Trim(end->raw) != "[end]"))
            ++end;
        if (end != lines_.end())
            ++end;
        at = lines_.erase(at, end);
        found = true;
    }
    return found;
}
std::string KvDocument::Serialize() const {
    std::string text;
    for (const auto& line : lines_) {
        text += line.raw;
        text += '\n';
    }
    return text;
}
std::vector<std::string> KvDocument::Sections() const {
    std::vector<std::string> result;
    for (const auto& line : lines_) {
        const std::string text = Trim(line.raw);
        if (!line.entry && !text.empty() && text[0] == '[' && text != "[end]")
            result.push_back(line.section);
    }
    return result;
}
std::vector<std::pair<std::string, std::string>> KvDocument::Entries(const std::string& section) const {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& line : lines_)
        if (line.entry && line.section == section)
            result.emplace_back(line.key, line.value);
    return result;
}
bool ParseUnsigned(const std::string& text, unsigned maximum, unsigned& out) {
    const std::string value = Trim(text);
    if (value.empty())
        return false;
    unsigned result = 0;
    for (char c : value) {
        if (c < '0' || c > '9')
            return false;
        const unsigned digit = static_cast<unsigned>(c - '0');
        if (digit > maximum || result > (maximum - digit) / 10)
            return false;
        result = result * 10 + digit;
    }
    out = result;
    return true;
}
} // namespace glide
