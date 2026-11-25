#pragma once
#include "request.h"
#include "resources.h"
#include "process.h"
#include <mutex>

namespace glide {
class LauncherSessions {
  public:
    LauncherSessions(std::wstring project, std::wstring binaries, unsigned webPort);
    ~LauncherSessions();
    LaunchReply Launch(const LaunchRequest& request);
    bool Reap(bool& empty, std::string& error);
    void Stop();

  private:
    struct Client {
        std::unique_ptr<PreparedRuntime> runtime;
        ProcessTree process;
        ~Client();
    };
    std::wstring project_, binaries_;
    unsigned webPort_;
    std::mutex mutex_;
    bool closing_ = false;
    std::vector<std::unique_ptr<Client>> clients_;
};
} // namespace glide
