#include "request.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include "glide/runtime_manifest.h"
#include <Windows.h>
#include <cwctype>

namespace glide {
bool LaunchReply::Encode(Buffer& out) const {
    std::wstring checked;
    if (error.size() > 4096 || !Utf8ToWide(error, checked) ||
        (error.empty() ? (!pid || !slot || slot > 999999) : (pid || slot)))
        return false;
    Buffer bytes;
    if (!bytes.U32(0x524c4147) || !bytes.U32(1) || !bytes.U32(pid) || !bytes.U32(slot) ||
        !bytes.U32(static_cast<uint32_t>(error.size())) || !bytes.Append(ByteView(error.data(), error.size())))
        return false;
    out = std::move(bytes);
    return true;
}
bool LaunchReply::Decode(ByteView bytes, LaunchReply& out) {
    if (!bytes.Valid() || bytes.size > 4116)
        return false;
    Reader reader(bytes);
    uint32_t magic = 0, version = 0, length = 0;
    ByteView text;
    LaunchReply reply;
    if (!reader.U32(magic) || magic != 0x524c4147 || !reader.U32(version) || version != 1 || !reader.U32(reply.pid) ||
        !reader.U32(reply.slot) || !reader.U32(length) || length > 4096 || !reader.Bytes(length, text) ||
        reader.Remaining())
        return false;
    reply.error.assign(reinterpret_cast<const char*>(text.data), text.size);
    Buffer checked;
    if (!reply.Encode(checked))
        return false;
    out = std::move(reply);
    return true;
}
namespace {
constexpr uint32_t Magic = 0x514c4147; // GALQ, little endian.
bool WriteString(Buffer& out, const std::wstring& value) {
    std::string text;
    return WideToUtf8(value, text) && text.size() <= 32768 && out.U32(static_cast<uint32_t>(text.size())) &&
           out.Append(ByteView(text.data(), text.size()));
}
bool ReadString(Reader& input, std::wstring& out) {
    uint32_t size = 0;
    ByteView bytes;
    return input.U32(size) && size <= 32768 && input.Bytes(size, bytes) &&
           Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.data), bytes.size), out);
}
} // namespace
bool LaunchRequest::Normalize(LaunchRequest& out, std::string& error) const {
    error = "Invalid launch request";
    if (!BuildTable::Find(build) || (overrideBuild && overrideBuild != build) || !port || executable.size() < 4 ||
        executable.size() >= 32768 || executable[1] != L':' ||
        !((executable[0] >= L'A' && executable[0] <= L'Z') || (executable[0] >= L'a' && executable[0] <= L'z')) ||
        (executable[2] != L'\\' && executable[2] != L'/') || executable.find(L'\0') != std::wstring::npos ||
        host.empty() || host.size() > 253)
        return false;
    LaunchRequest result = *this;
    for (auto& c : result.host) {
        if (!((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || (c >= L'0' && c <= L'9') || c == L'-' ||
              c == L'.'))
            return false;
        c = static_cast<wchar_t>(std::towlower(c));
    }
    // A trailing DNS root dot names the same host. Keep hostnames distinct from resolved IPs.
    if (result.host.back() == L'.')
        result.host.pop_back();
    if (result.host.empty())
        return false;
    size_t label = 0;
    for (size_t i = 0; i <= result.host.size(); ++i) {
        if (i != result.host.size() && result.host[i] != L'.')
            continue;
        if (i == label || i - label > 63 || result.host[label] == L'-' || result.host[i - 1] == L'-')
            return false;
        label = i + 1;
    }
    wchar_t full[32768];
    const DWORD count = GetFullPathNameW(executable.c_str(), 32768, full, nullptr);
    if (!count || count >= 32768 || !RuntimeRelativePath(std::wstring(full + 3)))
        return false;
    result.executable = full;
    out = std::move(result);
    error.clear();
    return true;
}
bool LaunchRequest::Encode(Buffer& out, std::string& error) const {
    LaunchRequest value;
    if (!Normalize(value, error))
        return false;
    Buffer bytes;
    if (!bytes.U32(Magic) || !bytes.U32(1) || !bytes.U32(value.build) || !bytes.U32(value.overrideBuild) ||
        !bytes.U32(value.windowed ? 1u : 0u) || !bytes.U16(value.port) || !WriteString(bytes, value.executable) ||
        !WriteString(bytes, value.host)) {
        error = "Cannot encode launch request";
        return false;
    }
    out = std::move(bytes);
    error.clear();
    return true;
}
bool LaunchRequest::Decode(ByteView bytes, LaunchRequest& out, std::string& error) {
    error = "Malformed or unsupported launch request";
    if (!bytes.Valid() || bytes.size > 65536)
        return false;
    Reader reader(bytes);
    uint32_t magic = 0, version = 0, flags = 0;
    LaunchRequest value;
    if (!reader.U32(magic) || magic != Magic || !reader.U32(version) || version != 1 || !reader.U32(value.build) ||
        !reader.U32(value.overrideBuild) || !reader.U32(flags) || flags > 1 || !reader.U16(value.port) ||
        !ReadString(reader, value.executable) || !ReadString(reader, value.host) || reader.Remaining())
        return false;
    value.windowed = flags != 0;
    return value.Normalize(out, error);
}
bool LaunchRequest::Identity(std::wstring& out, std::string& error) const {
    LaunchRequest value;
    if (!Normalize(value, error))
        return false;
    for (auto& c : value.executable)
        c = static_cast<wchar_t>(std::towlower(c));
    // Window mode and an explicit matching override do not select a different supervisor.
    value.windowed = false;
    value.overrideBuild = 0;
    Buffer bytes;
    uint8_t digest[32];
    if (!value.Encode(bytes, error) || !Sha256(bytes.View(), digest)) {
        error = "Cannot identify supervisor";
        return false;
    }
    std::wstring identity;
    for (uint8_t byte : digest) {
        identity += L"0123456789abcdef"[byte >> 4];
        identity += L"0123456789abcdef"[byte & 15];
    }
    out = std::move(identity);
    error.clear();
    return true;
}
bool LaunchRequest::VerifyExecutable(const BuildInfo*& out, std::string& error) const {
    LaunchRequest value;
    if (!Normalize(value, error))
        return false;
    PeImage image;
    const BuildInfo* detected = nullptr;
    if (!PeImage::FromFile(value.executable.c_str(), image) ||
        DetectBuild(image, value.overrideBuild, detected) != BuildDetection::Found || detected->build != value.build) {
        error = "Requested executable does not match its detected build and architecture";
        return false;
    }
    out = detected;
    error.clear();
    return true;
}
} // namespace glide
