#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace glide {
// Holds originals read-only until all clients using them have terminated.
class ReadOnlyResources {
  public:
    ~ReadOnlyResources();
    ReadOnlyResources() = default;
    ReadOnlyResources(const ReadOnlyResources&) = delete;
    ReadOnlyResources& operator=(const ReadOnlyResources&) = delete;
    bool Acquire(const std::vector<std::wstring>& paths, std::string& error);

  private:
    struct ParallelTree;
    static DWORD WINAPI LockChildTrees(void* parameter);
    bool HoldChildTrees(const std::vector<std::wstring>& paths);
    bool AddTree(const std::wstring& path, unsigned depth);
    bool Hold(const std::wstring& path, bool directory);
    void Release();
    std::vector<HANDLE> handles_;
};
} // namespace glide
