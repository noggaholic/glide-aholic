#pragma once
#include "glide/runtime_manifest.h"
#include "glide/resource_locks.h"
#include <Windows.h>
#include <string>
#include <vector>

namespace glide {
// Keep alive until the supervised process tree has terminated.
class RuntimeSlot {
  public:
    ~RuntimeSlot();
    RuntimeSlot() = default;
    RuntimeSlot(const RuntimeSlot&) = delete;
    RuntimeSlot& operator=(const RuntimeSlot&) = delete;
    bool Reserve(const std::wstring& runtimeRoot, unsigned build, std::string& error,
                 const std::string& identity = std::string());
    bool CopyResource(const std::wstring& source, const std::wstring& relative, std::string& error);
    bool PrepareUserData(std::string& error);
    const std::wstring& Directory() const {
        return directory_;
    }
    unsigned Number() const {
        return number_;
    }
    const std::wstring& JobName() const {
        return jobName_;
    }

  private:
    bool HoldDirectory(const std::wstring& path, bool create);
    void Release();
    HANDLE reservation_ = INVALID_HANDLE_VALUE;
    std::vector<HANDLE> directories_;
    std::wstring directory_;
    std::wstring jobName_;
    unsigned number_ = 0;
};
} // namespace glide
