#pragma once
#include "glide/buffer.h"
#include <winsock2.h>
#include <string>
#include <atomic>

namespace glide {
class ByteStream {
  public:
    virtual ~ByteStream() {}
    virtual bool Read(uint8_t* data, size_t capacity, size_t& received) = 0;
    virtual bool Write(ByteView data) = 0;
};

class Network {
  public:
    Network();
    ~Network();
    Network(const Network&) = delete;
    Network& operator=(const Network&) = delete;
    bool Ready() const {
        return ready_;
    }

  private:
    bool ready_;
};

class Socket : public ByteStream {
  public:
    Socket() : handle_(INVALID_SOCKET) {}
    explicit Socket(SOCKET handle) : handle_(handle) {}
    ~Socket();
    Socket(Socket&& other) noexcept;
    Socket& operator=(Socket&& other) noexcept;
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    bool Connect(const std::string& host, uint16_t port);
    bool Listen(const std::string& host, uint16_t port);
    bool Accept(Socket& out);
    bool WaitReadable(unsigned milliseconds) const;
    bool SetTimeout(unsigned milliseconds);
    bool SetNoDelay(bool enabled);
    uint16_t LocalPort() const;
    bool Valid() const {
        return handle_ != INVALID_SOCKET;
    }
    bool Stopped() const {
        return stopped_;
    }
    SOCKET Native() const {
        return handle_;
    }
    bool Read(uint8_t* data, size_t capacity, size_t& received) override;
    bool Write(ByteView data) override;
    void Shutdown();
    void Close();

  private:
    SOCKET handle_;
    std::atomic<bool> stopped_{false};
    unsigned timeout_ = 0;
};

bool ReadExact(ByteStream& stream, uint8_t* data, size_t count);
} // namespace glide
