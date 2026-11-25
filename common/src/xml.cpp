#include "glide/xml.h"
#include "glide/files.h"
#include <cstring>

namespace glide {
bool XmlValue(ByteView xml, const char* tag, size_t limit, std::string& out) {
    if (!xml.Valid() || !tag || !*tag || xml.size > 1024 * 1024)
        return false;
    const std::string text(xml.size ? reinterpret_cast<const char*>(xml.data) : "", xml.size);
    const std::string open = std::string("<") + tag + ">", close = std::string("</") + tag + ">";
    const size_t begin = text.find(open);
    if (begin == std::string::npos || text.find(open, begin + open.size()) != std::string::npos)
        return false;
    const size_t start = begin + open.size(), end = text.find(close, start);
    if (end == std::string::npos)
        return false;
    std::string decoded;
    for (size_t at = start; at < end;) {
        char c = text[at++];
        if (c == '<' || c == '\0')
            return false;
        if (c == '&') {
            const struct Entity {
                const char* encoded;
                char decoded;
            } entities[] = {{"amp;", '&'}, {"lt;", '<'}, {"gt;", '>'}, {"quot;", '"'}, {"apos;", '\''}};
            bool found = false;
            for (const auto& entity : entities) {
                const size_t size = std::strlen(entity.encoded);
                if (size <= end - at && text.compare(at, size, entity.encoded) == 0) {
                    c = entity.decoded;
                    at += size;
                    found = true;
                    break;
                }
            }
            if (!found)
                return false;
        }
        if (decoded.size() == limit)
            return false;
        decoded += c;
    }
    std::wstring checked;
    if (!Utf8ToWide(decoded, checked))
        return false;
    out = std::move(decoded);
    return true;
}
std::string XmlEscape(const std::string& text) {
    std::string escaped;
    for (char c : text) {
        switch (c) {
        case '&':
            escaped += "&amp;";
            break;
        case '<':
            escaped += "&lt;";
            break;
        case '>':
            escaped += "&gt;";
            break;
        case '"':
            escaped += "&quot;";
            break;
        case '\'':
            escaped += "&apos;";
            break;
        default:
            escaped += c;
            break;
        }
    }
    return escaped;
}
} // namespace glide
