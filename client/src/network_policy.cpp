#include "network_policy.h"
#include <ws2tcpip.h>
#include <cstring>
#include <cwchar>

namespace glide {
namespace {
std::string Lower(std::string value) {
    for (auto& c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c + ('a' - 'A'));
    return value;
}
bool Prefix(const std::string& text, const char* prefix) {
    return text.compare(0, std::strlen(prefix), prefix) == 0;
}
} // namespace
bool NetworkPolicy::Configure(const std::string& serverAddress, unsigned gamePort, unsigned webPort,
                              const std::string& machineName) {
    in_addr server = {};
    if (!gamePort || gamePort > 65535 || !webPort || webPort > 65535 || webPort == 6112 || webPort == 6600 ||
        gamePort == webPort || serverAddress.find('\0') != std::string::npos ||
        machineName.find('\0') != std::string::npos || InetPtonA(AF_INET, serverAddress.c_str(), &server) != 1)
        return false;
    const ULONG address = ntohl(server.s_addr);
    if (!address || (address >> 24) == 0 || (address >> 28) >= 14)
        return false;
    if (address == INADDR_LOOPBACK && gamePort == 80)
        return false; // Legacy CDN traffic uses loopback:80.
    server_ = server;
    gamePort_ = static_cast<unsigned short>(gamePort);
    webPort_ = static_cast<unsigned short>(webPort);
    machine_ = Lower(machineName);
    return true;
}
HostTarget NetworkPolicy::Classify(const char* host) const {
    if (!gamePort_)
        return HostTarget::Denied;
    if (!host)
        return HostTarget::Passive;
    const std::string name = Lower(host);
    if (name == "..localmachine" || (!machine_.empty() && name == machine_))
        return HostTarget::Machine;
    if (name == "localhost" || name == "127.0.0.1")
        return HostTarget::Loopback;
    if (Prefix(name, "cligate.") || Prefix(name, "cligate-pvg.") || Prefix(name, "auth1.") || Prefix(name, "auth2.") ||
        name == "auth7.111.cgw2.com" || name == "auth7.111.cgw2.com." || name == "auth9.111.cgw2.com" ||
        name == "auth9.111.cgw2.com.")
        return HostTarget::Game;
    if (Prefix(name, "assetcdn.") || name == "assetcdn-b.111.cgw2.com")
        return HostTarget::Web;
    in_addr numeric = {};
    if (InetPtonA(AF_INET, host, &numeric) == 1)
        return HostTarget::Numeric;
    return HostTarget::Denied;
}
bool NetworkPolicy::Route(const sockaddr* source, int size, sockaddr_in& target) const {
    if (!gamePort_ || !source || size < static_cast<int>(sizeof(sockaddr_in)))
        return false;
    sockaddr_in address;
    std::memcpy(&address, source, sizeof(address));
    if (address.sin_family != AF_INET)
        return false;
    const unsigned short port = ntohs(address.sin_port);
    if (port == 6112 || port == 6600) {
        address.sin_addr = server_;
        address.sin_port = htons(gamePort_);
    } else if (address.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) {
        if (port == 80)
            address.sin_port = htons(webPort_);
        else if (port == gamePort_)
            address.sin_addr = server_;
        else if (port != webPort_)
            return false;
    } else if (address.sin_addr.s_addr != server_.s_addr || port != gamePort_)
        return false;
    target = address;
    return true;
}
bool UpdaterTemporaryPath(const wchar_t* path, size_t length) {
    if (!path)
        return false;
    size_t start = length, end = length;
    while (start && path[start - 1] != L'\\' && path[start - 1] != L'/')
        --start;
    for (size_t i = start; i < length; ++i)
        if (path[i] == L':') {
            end = i;
            break;
        }
    while (end > start && (path[end - 1] == L'.' || path[end - 1] == L' '))
        --end;
    return (end - start == 10 && !_wcsnicmp(path + start, L"Gw2-64.tmp", 10)) ||
           (end - start == 7 && !_wcsnicmp(path + start, L"Gw2.tmp", 7));
}
} // namespace glide
