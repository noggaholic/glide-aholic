#include "glide/sts.h"
#include <cstdio>
#include <cstring>
#include <utility>

namespace glide {
namespace {
bool Decimal(const std::string& text, size_t limit, size_t& value) {
    if (text.empty())
        return false;
    size_t result = 0;
    for (char c : text) {
        if (c < '0' || c > '9')
            return false;
        const size_t digit = static_cast<size_t>(c - '0');
        if (digit > limit || result > (limit - digit) / 10)
            return false;
        result = result * 10 + digit;
    }
    value = result;
    return true;
}
bool HeaderText(const std::string& text) {
    for (unsigned char c : text)
        if (c < 32 || c == 127)
            return false;
    return true;
}
} // namespace

DecodeStatus DecodeStsRequest(ByteView input, StsRequest& out) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    size_t end = SIZE_MAX;
    for (size_t i = 0; i <= kStsMaxHeader && i <= input.size && input.size - i >= 4; ++i) {
        if (std::memcmp(input.data + i, "\r\n\r\n", 4) == 0) {
            end = i;
            break;
        }
    }
    if (end == SIZE_MAX)
        return input.size <= kStsMaxHeader + 3 ? DecodeStatus::Incomplete : DecodeStatus::Malformed;
    const std::string header(reinterpret_cast<const char*>(input.data), end);
    if (header.find('\0') != std::string::npos)
        return DecodeStatus::Malformed;
    const size_t firstEnd = header.find("\r\n");
    if (firstEnd == std::string::npos)
        return DecodeStatus::Malformed;
    const std::string first = header.substr(0, firstEnd);
    const std::string suffix = " STS/1.0";
    if (first.size() <= 2 + suffix.size() || first.compare(0, 2, "P ") != 0 ||
        first.compare(first.size() - suffix.size(), suffix.size(), suffix) != 0)
        return DecodeStatus::Malformed;
    StsRequest request;
    request.path = first.substr(2, first.size() - 2 - suffix.size());
    if (request.path.size() >= 160 || request.path[0] != '/' || request.path.find(' ') != std::string::npos ||
        !HeaderText(request.path))
        return DecodeStatus::Malformed;
    size_t bodySize = 0;
    bool hasLength = false, hasSequence = false;
    size_t at = firstEnd + 2;
    while (at < header.size()) {
        size_t next = header.find("\r\n", at);
        if (next == std::string::npos)
            next = header.size();
        const std::string line = header.substr(at, next - at);
        const size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0)
            return DecodeStatus::Malformed;
        const std::string name = line.substr(0, colon);
        size_t start = colon + 1;
        while (start < line.size() && (line[start] == ' ' || line[start] == '\t'))
            ++start;
        const std::string value = line.substr(start);
        if (!HeaderText(name) || !HeaderText(value))
            return DecodeStatus::Malformed;
        if (name == "l" || name == "L") {
            if (hasLength || !Decimal(value, kStsMaxBody, bodySize))
                return DecodeStatus::Malformed;
            hasLength = true;
        } else if (name == "s" || name == "S") {
            if (hasSequence)
                return DecodeStatus::Malformed;
            request.sequence = value.substr(0, value.find(';'));
            if (request.sequence.size() >= 64)
                return DecodeStatus::Malformed;
            hasSequence = true;
        }
        at = next + 2;
    }
    if (!hasLength)
        return DecodeStatus::Malformed;
    const size_t consumed = end + 4 + bodySize;
    if (input.size < consumed)
        return DecodeStatus::Incomplete;
    request.body = ByteView(input.data + end + 4, bodySize);
    request.consumed = consumed;
    out = std::move(request);
    return DecodeStatus::Complete;
}

bool EncodeStsResponseHeader(const std::string& sequence, unsigned status, const std::string& statusText,
                             size_t bodySize, Buffer& out) {
    if (bodySize > kStsMaxBody || sequence.size() >= 64 || !HeaderText(sequence) || statusText.empty() ||
        !HeaderText(statusText))
        return false;
    if (sequence.empty())
        return true;
    char header[256];
    const int size = std::snprintf(header, sizeof(header), "STS/1.0 %u %s\r\ns:%sR\r\nl:%u\r\n\r\n", status,
                                   statusText.c_str(), sequence.c_str(), static_cast<unsigned>(bodySize));
    return size > 0 && static_cast<size_t>(size) < sizeof(header) &&
           out.Append(ByteView(header, static_cast<size_t>(size)));
}
bool DecodeStsPassword(const std::string& encoded, std::string& out) {
    if (encoded.empty() || encoded.size() > 5464 || encoded.size() % 4)
        return false;
    const auto digit = [](char c) -> int {
        if (c >= 'A' && c <= 'Z')
            return c - 'A';
        if (c >= 'a' && c <= 'z')
            return c - 'a' + 26;
        if (c >= '0' && c <= '9')
            return c - '0' + 52;
        return c == '+' ? 62 : c == '/' ? 63 : -1;
    };
    std::string decoded;
    for (size_t at = 0; at < encoded.size(); at += 4) {
        const int a = digit(encoded[at]), b = digit(encoded[at + 1]);
        const bool pad2 = encoded[at + 2] == '=', pad3 = encoded[at + 3] == '=';
        const int c = pad2 ? 0 : digit(encoded[at + 2]);
        const int d = pad3 ? 0 : digit(encoded[at + 3]);
        if (a < 0 || b < 0 || c < 0 || d < 0 || (pad2 && !pad3) || ((pad2 || pad3) && at + 4 != encoded.size()) ||
            (pad2 && (b & 15)) || (pad3 && !pad2 && (c & 3)))
            return false;
        decoded += static_cast<char>((a << 2) | (b >> 4));
        if (!pad2)
            decoded += static_cast<char>((b << 4) | (c >> 2));
        if (!pad3)
            decoded += static_cast<char>((c << 6) | d);
    }
    if (decoded.size() > 4096 || decoded.find('\0') != std::string::npos ||
        decoded.find_first_of("\r\n") != std::string::npos)
        return false;
    out = std::move(decoded);
    return true;
}
} // namespace glide
