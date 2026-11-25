#pragma once
#include "request.h"
#include <Windows.h>
#include <functional>

namespace glide {
// Handler runs serially on the pipe worker. Stop waits for any active handler to finish.
class SupervisorPipe {
  public:
    using Handler = std::function<LaunchReply(const LaunchRequest&)>;
    ~SupervisorPipe();
    SupervisorPipe() = default;
    SupervisorPipe(const SupervisorPipe&) = delete;
    SupervisorPipe& operator=(const SupervisorPipe&) = delete;
    bool Start(const LaunchRequest& owner, Handler handler, std::string& error, bool* existing = nullptr);
    void Stop();
    static bool Forward(const LaunchRequest& request, LaunchReply& reply, DWORD timeoutMs, std::string& error);
    static std::wstring PipeName(const std::wstring& identity);

  private:
    static DWORD WINAPI Thread(void* self);
    void Run();
    HANDLE mutex_ = nullptr, stop_ = nullptr, thread_ = nullptr, pipe_ = INVALID_HANDLE_VALUE;
    std::wstring identity_;
    Handler handler_;
};
} // namespace glide
