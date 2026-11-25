#include "glide/socket.h"
#include <ws2tcpip.h>
#include <climits>
#include <utility>

namespace glide {
namespace {
bool NonBlocking(SOCKET handle) {
    u_long enabled = 1;
    return ioctlsocket(handle, FIONBIO, &enabled) == 0;
}
} // namespace
Network::Network() : ready_(false) {
    WSADATA data;
    ready_ = WSAStartup(MAKEWORD(2, 2), &data) == 0;
}
Network::~Network() {
    if (ready_)
        WSACleanup();
}
Socket::~Socket() {
    Close();
}
Socket::Socket(Socket&& other) noexcept
    : handle_(other.handle_), stopped_(other.stopped_.load()), timeout_(other.timeout_) {
    other.handle_ = INVALID_SOCKET;
}
Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        handle_ = other.handle_;
        stopped_ = other.stopped_.load();
        timeout_ = other.timeout_;
        other.handle_ = INVALID_SOCKET;
    }
    return *this;
}
void Socket::Close() {
    if (Valid()) {
        closesocket(handle_);
        handle_ = INVALID_SOCKET;
    }
}
void Socket::Shutdown() {
    stopped_ = true;
    if (Valid())
        shutdown(handle_, SD_BOTH);
}
bool Socket::SetTimeout(unsigned milliseconds) {
    if (!Valid())
        return false;
    timeout_ = milliseconds;
    const DWORD timeout = milliseconds;
    return setsockopt(handle_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) ==
               0 &&
           setsockopt(handle_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
}
bool Socket::SetNoDelay(bool enabled) {
    if (!Valid())
        return false;
    const BOOL value = enabled ? TRUE : FALSE;
    return setsockopt(handle_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&value), sizeof(value)) == 0;
}
bool Socket::Connect(const std::string& host, uint16_t port) {
    if (Valid() || host.empty() || port == 0)
        return false;
    addrinfo hints = {}, *addresses = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &addresses))
        return false;
    struct Free {
        addrinfo* data;
        ~Free() {
            freeaddrinfo(data);
        }
    } free{addresses};
    for (auto* at = addresses; at; at = at->ai_next) {
        Socket candidate(socket(at->ai_family, at->ai_socktype, at->ai_protocol));
        if (candidate.Valid() && connect(candidate.Native(), at->ai_addr, static_cast<int>(at->ai_addrlen)) == 0) {
            *this = std::move(candidate);
            return true;
        }
    }
    return false;
}
bool Socket::Listen(const std::string& host, uint16_t port) {
    if (Valid())
        return false;
    addrinfo hints = {}, *addresses = nullptr;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo(host.empty() ? nullptr : host.c_str(), std::to_string(port).c_str(), &hints, &addresses))
        return false;
    struct Free {
        addrinfo* data;
        ~Free() {
            freeaddrinfo(data);
        }
    } free{addresses};
    for (auto* at = addresses; at; at = at->ai_next) {
        Socket candidate(socket(at->ai_family, at->ai_socktype, at->ai_protocol));
        const BOOL exclusive = TRUE;
        if (candidate.Valid() &&
            setsockopt(candidate.Native(), SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive),
                       sizeof(exclusive)) == 0 &&
            bind(candidate.Native(), at->ai_addr, static_cast<int>(at->ai_addrlen)) == 0 &&
            listen(candidate.Native(), SOMAXCONN) == 0) {
            *this = std::move(candidate);
            return true;
        }
    }
    return false;
}
bool Socket::Accept(Socket& out) {
    if (!Valid())
        return false;
    Socket accepted(accept(handle_, nullptr, nullptr));
    if (!accepted.Valid())
        return false;
    out = std::move(accepted);
    return true;
}
bool Socket::WaitReadable(unsigned milliseconds) const {
    if (!Valid())
        return false;
    fd_set read;
    FD_ZERO(&read);
    FD_SET(handle_, &read);
    timeval timeout{static_cast<long>(milliseconds / 1000), static_cast<long>((milliseconds % 1000) * 1000)};
    return select(0, &read, nullptr, nullptr, &timeout) > 0;
}
uint16_t Socket::LocalPort() const {
    sockaddr_storage address = {};
    int size = sizeof(address);
    if (getsockname(handle_, reinterpret_cast<sockaddr*>(&address), &size))
        return 0;
    if (address.ss_family == AF_INET)
        return ntohs(reinterpret_cast<sockaddr_in*>(&address)->sin_port);
    if (address.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<sockaddr_in6*>(&address)->sin6_port);
    return 0;
}
bool Socket::Read(uint8_t* data, size_t capacity, size_t& received) {
    received = 0;
    if (stopped_ || !Valid() || !data || capacity == 0 || !NonBlocking(handle_))
        return false;
    const ULONGLONG start = GetTickCount64();
    for (;;) {
        if (stopped_)
            return false;
        fd_set readable;
        FD_ZERO(&readable);
        FD_SET(handle_, &readable);
        timeval interval{0, 100000};
        const int ready = select(0, &readable, nullptr, nullptr, &interval);
        if (ready < 0)
            return false;
        if (ready > 0) {
            if (stopped_)
                return false;
            const int count = recv(handle_, reinterpret_cast<char*>(data),
                                   static_cast<int>(capacity > INT_MAX ? INT_MAX : capacity), 0);
            if (count > 0) {
                received = static_cast<size_t>(count);
                return true;
            }
            if (count == 0 || WSAGetLastError() != WSAEWOULDBLOCK)
                return false;
        }
        if (timeout_ && GetTickCount64() - start >= timeout_)
            return false;
    }
}
bool Socket::Write(ByteView data) {
    if (stopped_ || !Valid() || !data.Valid() || !NonBlocking(handle_))
        return false;
    size_t at = 0;
    const ULONGLONG start = GetTickCount64();
    while (at < data.size) {
        if (stopped_)
            return false;
        if (timeout_ && GetTickCount64() - start >= timeout_)
            return false;
        fd_set writable;
        FD_ZERO(&writable);
        FD_SET(handle_, &writable);
        timeval interval{0, 100000};
        const int ready = select(0, nullptr, &writable, nullptr, &interval);
        if (ready < 0)
            return false;
        if (!ready)
            continue;
        if (stopped_)
            return false;
        const size_t remaining = data.size - at;
        const int count = send(handle_, reinterpret_cast<const char*>(data.data + at),
                               static_cast<int>(remaining > INT_MAX ? INT_MAX : remaining), 0);
        if (count <= 0) {
            if (count < 0 && WSAGetLastError() == WSAEWOULDBLOCK)
                continue;
            return false;
        }
        at += static_cast<size_t>(count);
    }
    return true;
}
bool ReadExact(ByteStream& stream, uint8_t* data, size_t count) {
    if (!data && count)
        return false;
    size_t at = 0;
    while (at < count) {
        size_t received = 0;
        if (!stream.Read(data + at, count - at, received) || !received || received > count - at)
            return false;
        at += received;
    }
    return true;
}
} // namespace glide
