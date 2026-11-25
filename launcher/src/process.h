#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace glide {
std::wstring QuoteArgument(const std::wstring& argument);
class ProcessTree {
  public:
    ProcessTree() = default;
    ~ProcessTree();
    ProcessTree(const ProcessTree&) = delete;
    ProcessTree& operator=(const ProcessTree&) = delete;
    bool StartSuspended(const std::wstring& executable, const std::vector<std::wstring>& arguments,
                        const std::wstring& directory, const std::vector<wchar_t>& environment, std::string& error,
                        const std::wstring& jobName = std::wstring());
    bool Resume(std::string& error);
    bool Active(unsigned& count) const;
    bool Stop(DWORD timeoutMs = 5000);
    HANDLE Process() const {
        return process_;
    }
    DWORD Pid() const {
        return pid_;
    }

  private:
    HANDLE job_ = nullptr, process_ = nullptr, thread_ = nullptr;
    DWORD pid_ = 0;
    bool resumed_ = false;
};
} // namespace glide
