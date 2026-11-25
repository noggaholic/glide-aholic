#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
bool EnsureDirectory(const std::wstring& path);
bool ReadFileBytes(const std::wstring& path, size_t limit, Buffer& out);
bool WriteFileAtomic(const std::wstring& path, ByteView data, bool replace);
bool Utf8ToWide(const std::string& input, std::wstring& out);
bool WideToUtf8(const std::wstring& input, std::string& out);

class DirectoryLock {
  public:
    DirectoryLock() : handle_(nullptr) {}
    ~DirectoryLock();
    DirectoryLock(const DirectoryLock&) = delete;
    DirectoryLock& operator=(const DirectoryLock&) = delete;
    bool Acquire(const std::wstring& directory);
    void Release();
    bool Held() const {
        return handle_ != nullptr;
    }

  private:
    void* handle_;
};
} // namespace glide
