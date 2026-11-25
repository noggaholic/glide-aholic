#include "glide/config.h"
#include "glide/files.h"
#include "glide/kv.h"
#include <utility>

namespace glide {
bool LoadServerConfig(const std::wstring& path, ServerConfig& out, std::string& error) {
    Buffer bytes;
    if (!ReadFileBytes(path, 65536, bytes)) {
        error = "Cannot read server configuration";
        return false;
    }
    KvDocument document;
    const std::string text(bytes.Size() ? reinterpret_cast<const char*>(bytes.Data()) : "", bytes.Size());
    if (!KvDocument::Parse(text, document, error))
        return false;
    ServerConfig config = out;
    std::string value;
    if (document.Count("", "host")) {
        if (!document.Get("", "host", value) || value.empty()) {
            error = "Invalid host";
            return false;
        }
        config.host = value;
    }
    if (document.Count("", "port")) {
        unsigned port = 0;
        if (!document.Get("", "port", value) || !ParseUnsigned(value, 65535, port)) {
            error = "Invalid port";
            return false;
        }
        config.port = static_cast<uint16_t>(port);
    }
    const struct Field {
        const char* key;
        std::wstring* value;
    } fields[] = {{"data_dir", &config.dataDir}, {"state_dir", &config.stateDir}, {"log_file", &config.logFile}};
    for (const auto& field : fields) {
        if (document.Count("", field.key) &&
            (!document.Get("", field.key, value) || value.empty() || !Utf8ToWide(value, *field.value))) {
            error = std::string("Invalid ") + field.key;
            return false;
        }
    }
    // File paths are relative to the configuration file, never the caller's working directory.
    const size_t slash = path.find_last_of(L"/\\");
    const std::wstring parent = slash == std::wstring::npos ? L"." : path.substr(0, slash);
    for (const auto& field : fields) {
        const auto& valuePath = *field.value;
        if (valuePath[0] != L'/' && valuePath[0] != L'\\' && !(valuePath.size() > 1 && valuePath[1] == L':'))
            *field.value = parent + L"/" + valuePath;
    }
    out = std::move(config);
    error.clear();
    return true;
}
} // namespace glide
