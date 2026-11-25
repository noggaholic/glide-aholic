#include "glide/log.h"
#include <Windows.h>
#include <share.h>

namespace glide {
Log::~Log() {
    if (file_)
        std::fclose(file_);
}
bool Log::Open(const std::wstring& path) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (file_)
        return false;
    file_ = _wfsopen(path.c_str(), L"ab", _SH_DENYWR);
    return file_ != nullptr;
}
void Log::Write(const char* component, const std::string& message) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (observer_)
        observer_(component, message);
    SYSTEMTIME now;
    GetLocalTime(&now);
    char prefix[128];
    std::snprintf(prefix, sizeof(prefix), "%02u:%02u:%02u.%03u [%s] ", now.wHour, now.wMinute, now.wSecond,
                  now.wMilliseconds, component);
    std::printf("%s%s\n", prefix, message.c_str());
    std::fflush(stdout);
    if (file_) {
        std::fprintf(file_, "%s%s\n", prefix, message.c_str());
        std::fflush(file_);
    }
}
} // namespace glide
