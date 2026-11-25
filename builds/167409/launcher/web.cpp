#include "manifest_pins.h"
#include "../../../launcher/src/web.h"
#include "../../../launcher/src/login_page.h"
#include "glide/crypto.h"
#include "glide/files.h"
#include <cstring>
namespace glide {
namespace {
bool ChinaPath(const china167409::ChinaManifestPin& pin, const std::string& path, bool& compressed) {
    const std::string base = "/program/111/0/0/" + std::to_string(pin.file);
    if (path == base) {
        compressed = false;
        return pin.sizes[0] != 0;
    }
    if (path == base + "/compressed") {
        compressed = true;
        return pin.sizes[1] != 0;
    }
    return false;
}
WebReply ChinaManifest(const china167409::ChinaManifestPin& pin, bool compressed, const std::wstring& directory) {
    WebReply result;
    result.status = 503;
    result.body = "China regional metadata is missing or changed";
    Buffer bytes;
    uint8_t digest[32];
    const std::wstring file = directory + L"/resources/" + std::to_wstring(pin.file) + (compressed ? L".cmp" : L".raw");
    if (!ReadFileBytes(file, pin.sizes[compressed], bytes) || bytes.Size() != pin.sizes[compressed] ||
        !Sha256(bytes.View(), digest))
        return result;
    std::string hex;
    for (uint8_t value : digest) {
        hex += "0123456789abcdef"[value >> 4];
        hex += "0123456789abcdef"[value & 15];
    }
    if (hex != pin.sha256[compressed])
        return result;
    result.status = 200;
    result.type = "application/binary";
    result.body.assign(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    result.headers = "X-Arena-Checksum-Raw: " + std::to_string(pin.rawChecksum) +
                     "\r\nX-Arena-Checksum-Ext: " + std::to_string(pin.extendedChecksum) +
                     "\r\nX-Arena-Checksum-Length: " + std::to_string(bytes.Size()) + "\r\nContent-Range: bytes 0-" +
                     std::to_string(bytes.Size() - 1) + "/" + std::to_string(bytes.Size()) + "\r\n";
    return result;
}
} // namespace
WebReply ChinaWebContent(const std::string& path, const std::wstring& directory) {
    using namespace china167409;
    WebReply result;
    if (path == "/launcher/167409/login") {
        result.status = 200;
        result.type = "text/html; charset=utf-8";
        result.body = kLocalLoginPage;
        return result;
    }
    if (path == "/latest64/111") {
        if (ChinaManifest(china_derived_root, false, directory).status != 200) {
            result.status = 503;
            result.body = "China regional root metadata is missing or changed";
            return result;
        }
        result.status = 200;
        result.body = "167409 2552332 44775192 2552327 20212";
        return result;
    }
    bool compressed = false;
    if (ChinaPath(china_derived_root, path, compressed))
        return ChinaManifest(china_derived_root, compressed, directory);
    for (const auto& pin : china_derived_children)
        if (ChinaPath(pin, path, compressed))
            return ChinaManifest(pin, compressed, directory);
    for (const auto& pin : china_current_optional_manifests)
        if (ChinaPath(pin, path, compressed))
            return ChinaManifest(pin, compressed, directory);
    return result;
}
} // namespace glide
