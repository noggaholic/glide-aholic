#include "../../builds/49284/launcher/current_build.h"
#include "glide/crypto.h"
#include <cstring>
#include "web.h"
#include "login_page.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <cstdio>

namespace glide {
namespace {
struct ManifestPin {
    uint32_t file_id;
    size_t sizes[2];
    const char* sha256[2];
    int32_t checksum_raw, checksum_ext;
};
#include "manifest_pins.h"
#include "compatible_manifest_pins.h"
const ManifestPin root = {3794236,
                          {20332, 7120},
                          {"bce1b5e38f584e943774bb0e1e6a6b7d144cd5ba312600deb9dad888cd22e16c",
                           "ad91747ea9f5598b6d5e49fc6a79a905de786c19ac7b3a0d8418fbf3de633c77"},
                          -199385128,
                          959375452};
bool Matches(const ManifestPin& pin, const std::string& path, bool& compressed) {
    const auto plain = "/program/101/1/0/" + std::to_string(pin.file_id);
    if (path == plain) {
        compressed = false;
        return true;
    }
    if (path == plain + "/compressed") {
        compressed = true;
        return true;
    }
    return false;
}
WebReply Manifest(const ManifestPin& pin, bool compressed, const std::wstring& directory) {
    WebReply result;
    result.status = 503;
    result.body = "Pinned manifest missing or invalid";
    Buffer bytes;
    uint8_t digest[32];
    char hex[65];
    const auto path =
        directory + L"\\manifests\\" + std::to_wstring(pin.file_id) + (compressed ? L".compressed" : L".raw");
    if (!ReadFileBytes(path, pin.sizes[compressed], bytes) || bytes.Size() != pin.sizes[compressed] ||
        !Sha256(bytes.View(), digest))
        return result;
    for (size_t i = 0; i < 32; ++i)
        std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    if (std::string(hex) != pin.sha256[compressed])
        return result;
    result.status = 200;
    result.type = "application/binary";
    result.body.assign(reinterpret_cast<const char*>(bytes.Data()), bytes.Size());
    result.headers = "X-Arena-Checksum-Raw: " + std::to_string(pin.checksum_raw) +
                     "\r\nX-Arena-Checksum-Ext: " + std::to_string(pin.checksum_ext) +
                     "\r\nX-Arena-Checksum-Length: " + std::to_string(bytes.Size()) + "\r\nContent-Range: bytes 0-" +
                     std::to_string(bytes.Size() - 1) + "/" + std::to_string(bytes.Size()) + "\r\n";
    return result;
}
} // namespace
WebReply Build204489WebContent(const std::string& path, const std::wstring& directory) {
    WebReply result;
    if (path == "/launcher/204489/login") {
        result.status = 200;
        result.type = "text/html; charset=utf-8";
        result.body = kLocalLoginPage;
        return result;
    }
    const bool compatible = Manifest(compatible_root, false, directory).status == 200;
    if (path == "/latest64/101") {
        result.status = 200;
        result.body =
            "204489 3794241 43059592 3794236 " + std::to_string(compatible ? compatible_root.sizes[0] : root.sizes[0]);
        return result;
    }
    bool compressed = false;
    if (compatible) {
        if (Matches(compatible_root, path, compressed))
            return Manifest(compatible_root, compressed, directory);
        for (const auto& pin : compatible_secondary_manifests)
            if (Matches(pin, path, compressed))
                return Manifest(pin, compressed, directory);
        return result;
    }
    if (Matches(root, path, compressed))
        return Manifest(root, compressed, directory);
    for (const auto& pin : secondary_manifests)
        if (Matches(pin, path, compressed))
            return Manifest(pin, compressed, directory);
    return result;
}
WebReply HotWebContent(const std::string& path, const std::wstring& directory) {
    WebReply result;
    // HoT registers the same login/browserReady/play contract as the working page.
    if (path == "/launcher/49284/login") {
        result.status = 200;
        result.type = "text/html; charset=utf-8";
        result.body = kLocalLoginPage;
        return result;
    }
    if (path != "/latest/101")
        return result;
    Buffer rootBytes;
    uint8_t hash[32];

    const uint8_t expected[] = {0xd6, 0x13, 0x8c, 0x9f, 0x1e, 0xae, 0x93, 0x77, 0x58, 0x44, 0x3e,
                                0x96, 0xa2, 0x58, 0xeb, 0x49, 0xb6, 0xd8, 0xf5, 0x5c, 0x7f, 0x79,
                                0xab, 0x5b, 0xf7, 0x06, 0x3f, 0xd4, 0x76, 0x17, 0xbb, 0x43};
    result.status = 503;
    result.body = "HoT original root metadata is missing or invalid";
    if (!ReadFileBytes(directory + L"/resources/original-root.bin", 7164, rootBytes) || rootBytes.Size() != 7164 ||
        !Sha256(rootBytes.View(), hash) || std::memcmp(hash, expected, sizeof(hash)))
        return result;
    const Glide49284InstalledMetadata metadata = {49284, 22658552, 49284, 1000292, 7164};
    char descriptor[128];
    size_t size = 0;
    if (!glide_49284_current_descriptor(&metadata, 101, 49284, descriptor, sizeof(descriptor), &size))
        return result;
    result.status = 200;
    result.body.assign(descriptor, size);
    return result;
}
LauncherWeb::~LauncherWeb() {
    Stop();
}
bool LauncherWeb::Start(unsigned build, const std::wstring& directory, std::string& error) {
    error = "Cannot start launcher HTTP service";
    if (thread_ ||
        (build != 204489 && build != 14075 && build != 14505 && build != 14422 && build != 49284 && build != 14812 &&
         build != 167409) ||
        !network_.Ready())
        return false;
    if (build == 204489 && (Build204489WebContent("/program/101/1/0/3794236", directory).status != 200 ||
                            Build204489WebContent("/program/101/1/0/3794236/compressed", directory).status != 200)) {
        error = "Build 204489 root manifests are missing or invalid";
        return false;
    }
    if (build == 49284 && HotWebContent("/latest/101", directory).status != 200) {
        error = "HoT original root metadata is missing or invalid";
        return false;
    }
    if (build == 167409 && ChinaWebContent("/latest64/111", directory).status != 200) {
        error = "China regional root metadata is missing or invalid";
        return false;
    }
    if (!listener_.Listen("127.0.0.1", 0))
        return false;
    port_ = listener_.LocalPort();
    contentDirectory_ = directory;
    build_ = build;
    stopping_ = false;
    thread_ = CreateThread(nullptr, 0, Worker, this, 0, nullptr);
    if (!thread_) {
        listener_.Close();
        port_ = 0;
        return false;
    }
    error.clear();
    return true;
}
void LauncherWeb::Stop() {
    stopping_ = true;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        client_.Shutdown();
    }
    if (thread_) {
        WaitForSingleObject(thread_, INFINITE);
        CloseHandle(thread_);
        thread_ = nullptr;
    }
    listener_.Close();
    port_ = 0;
}
DWORD WINAPI LauncherWeb::Worker(void* context) {
    static_cast<LauncherWeb*>(context)->Run();
    return 0;
}
void LauncherWeb::Run() {
    while (!stopping_) {
        if (!listener_.WaitReadable(100))
            continue;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_ || !listener_.Accept(client_))
                continue;
            if (!client_.SetTimeout(2000)) {
                client_.Close();
                continue;
            }
        }
        std::string request;
        const ULONGLONG deadline = GetTickCount64() + 2000;
        while (!stopping_ && request.size() < 8192 && request.find("\r\n\r\n") == std::string::npos &&
               GetTickCount64() < deadline) {
            uint8_t bytes[1024];
            size_t read = 0;
            if (!client_.Read(bytes, sizeof(bytes), read))
                break;
            request.append(reinterpret_cast<const char*>(bytes), read);
        }
        WebReply reply;
        const size_t firstLine = request.find("\r\n");
        if (request.size() <= 8192 && request.find("\r\n\r\n") != std::string::npos && firstLine != std::string::npos) {
            const std::string line = request.substr(0, firstLine);
            const size_t end = line.find(' ', 4);
            if (line.compare(0, 4, "GET ") == 0 && end != std::string::npos &&
                (line.substr(end + 1) == "HTTP/1.1" || line.substr(end + 1) == "HTTP/1.0"))
                reply = build_ == 204489   ? Build204489WebContent(line.substr(4, end - 4), contentDirectory_)
                        : build_ == 49284  ? HotWebContent(line.substr(4, end - 4), contentDirectory_)
                        : build_ == 167409 ? ChinaWebContent(line.substr(4, end - 4), contentDirectory_)
                                           : WebReply();
        }
        const std::string status = reply.status == 200   ? "200 OK"
                                   : reply.status == 503 ? "503 Service Unavailable"
                                                         : "404 Not Found";
        const std::string header = "HTTP/1.1 " + status + "\r\nContent-Type: " + reply.type +
                                   "\r\nContent-Length: " + std::to_string(reply.body.size()) +
                                   "\r\nCache-Control: no-store\r\nConnection: close\r\n" + reply.headers + "\r\n";
        bool delivered = false;
        if (!stopping_ && client_.Write(ByteView(reinterpret_cast<const uint8_t*>(header.data()), header.size())))
            delivered = client_.Write(ByteView(reinterpret_cast<const uint8_t*>(reply.body.data()), reply.body.size()));
        if (build_ == 49284 && reply.status == 200 && reply.type == "text/html; charset=utf-8") {
            std::puts(delivered ? "[web] HoT local login page delivered" : "[web] HoT local login page write failed");
            std::fflush(stdout); // Fixed message only: never print request data or credentials.
        }
        {
            std::lock_guard<std::mutex> lock(mutex_);
            client_.Close();
        }
    }
}
} // namespace glide
