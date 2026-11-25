#pragma once
#include "glide/socket.h"
#include <Windows.h>
#include <mutex>

namespace glide {
struct WebReply {
    unsigned status = 404;
    std::string type = "text/plain", headers, body = "Not found";
};
WebReply HotWebContent(const std::string& path, const std::wstring& contentDirectory);
WebReply Build204489WebContent(const std::string& path, const std::wstring& contentDirectory);
WebReply ChinaWebContent(const std::string& path, const std::wstring& contentDirectory);
class LauncherWeb {
  public:
    ~LauncherWeb();
    bool Start(unsigned build, const std::wstring& contentDirectory, std::string& error);
    void Stop();
    uint16_t Port() const {
        return port_;
    }

  private:
    static DWORD WINAPI Worker(void* context);
    void Run();
    Network network_;
    Socket listener_, client_;
    std::mutex mutex_;
    std::atomic<bool> stopping_{false};
    HANDLE thread_ = nullptr;
    uint16_t port_ = 0;
    unsigned build_ = 0;
    std::wstring contentDirectory_;
};
} // namespace glide
