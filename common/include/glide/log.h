#pragma once
#include <cstdio>
#include <functional>
#include <mutex>
#include <string>
#include <utility>

namespace glide {
using LogObserver = std::function<void(const char*, const std::string&)>;
class Log {
  public:
    explicit Log(LogObserver observer = LogObserver()) : observer_(std::move(observer)) {}
    ~Log();
    bool Open(const std::wstring& path);
    void Write(const char* component, const std::string& message);

  private:
    std::mutex mutex_;
    LogObserver observer_;
    FILE* file_ = nullptr;
};
} // namespace glide
