#pragma once
#include <cstdint>
#include <string>

namespace glide {
struct ServerConfig {
    std::string host = "127.0.0.1";
    uint16_t port = 6112;
    std::wstring dataDir = L"data";
    std::wstring stateDir = L"state";
    std::wstring logFile = L"glide-server.log";
    uint32_t build = 0;
    bool verbose = false;
};
bool LoadServerConfig(const std::wstring& path, ServerConfig& out, std::string& error);
} // namespace glide
