#pragma once
#include <winsock2.h>
#include <string>

namespace glide {
enum class HostTarget { Denied, Passive, Machine, Loopback, Game, Web, Numeric };
class NetworkPolicy {
  public:
    bool Configure(const std::string& serverAddress, unsigned gamePort, unsigned webPort,
                   const std::string& machineName);
    HostTarget Classify(const char* host) const;
    bool Route(const sockaddr* source, int size, sockaddr_in& target) const;
    const in_addr& ServerAddress() const {
        return server_;
    }
    unsigned short GamePort() const {
        return gamePort_;
    }
    unsigned short WebPort() const {
        return webPort_;
    }

  private:
    in_addr server_ = {};
    unsigned short gamePort_ = 0, webPort_ = 0;
    std::string machine_;
};
bool UpdaterTemporaryPath(const wchar_t* path, size_t length);
} // namespace glide
