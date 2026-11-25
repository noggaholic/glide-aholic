#include "runtime.h"
#include "glide/crypto.h"
#include "glide/files.h"
#include <cwctype>
#include <cwchar>
#include <cstdint>
#include <cstring>

namespace glide {
namespace {
class Handle {
  public:
    explicit Handle(HANDLE value) : value_(value) {}
    ~Handle() {
        if (value_ != INVALID_HANDLE_VALUE)
            CloseHandle(value_);
    }
    HANDLE Get() const {
        return value_;
    }

  private:
    HANDLE value_;
};

} // namespace
RuntimeSlot::~RuntimeSlot() {
    Release();
}

void RuntimeSlot::Release() {
    if (reservation_ != INVALID_HANDLE_VALUE)
        CloseHandle(reservation_);
    reservation_ = INVALID_HANDLE_VALUE;
    for (auto at = directories_.rbegin(); at != directories_.rend(); ++at)
        CloseHandle(*at);
    directories_.clear();
    directory_.clear();
    jobName_.clear();
    number_ = 0;
}
bool RuntimeSlot::HoldDirectory(const std::wstring& path, bool create) {
    if (create && !CreateDirectoryW(path.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return false;
    HANDLE handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;
    BY_HANDLE_FILE_INFORMATION info = {};
    if (!GetFileInformationByHandle(handle, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        CloseHandle(handle);
        return false;
    }
    directories_.push_back(handle);
    return true;
}
bool RuntimeSlot::Reserve(const std::wstring& runtimeRoot, unsigned build, std::string& error,
                          const std::string& identity) {
    error = "Cannot reserve a private runtime directory";
    if (!identity.empty() &&
        (identity.size() != 64 || identity.find_first_not_of("0123456789abcdef") != std::string::npos))
        return false;
    if (reservation_ != INVALID_HANDLE_VALUE || !build || runtimeRoot.size() < 3 || runtimeRoot[1] != L':' ||
        (runtimeRoot[2] != L'\\' && runtimeRoot[2] != L'/') || runtimeRoot.find(L'\0') != std::wstring::npos)
        return false;
    wchar_t full[32768];
    const DWORD length = GetFullPathNameW(runtimeRoot.c_str(), 32768, full, nullptr);
    if (!length || length >= 32768)
        return false;
    std::wstring root(full);
    while (root.size() > 3 && root.back() == L'\\')
        root.pop_back();
    if (!RuntimeRelativePath(root.substr(3)))
        return false;
    if (!HoldDirectory(root.substr(0, 3), false))
        return false;
    for (size_t at = 3; at <= root.size(); ++at) {
        if (at != root.size() && root[at] != L'\\')
            continue;
        if (!HoldDirectory(root.substr(0, at), true)) {
            Release();
            return false;
        }
    }
    const std::wstring buildRoot = root + L"\\" + std::to_wstring(build);
    if (!HoldDirectory(buildRoot, true)) {
        Release();
        return false;
    }
    for (unsigned slot = 1; slot != 1000000; ++slot) {
        const std::wstring path = buildRoot + L"\\client-" + std::to_wstring(slot);
        if (!HoldDirectory(path, true)) {
            Release();
            return false;
        }
        HANDLE lease = CreateFileW((path + L"\\.slot.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                                   OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (lease != INVALID_HANDLE_VALUE) {
            std::wstring normalized(path);
            for (auto& c : normalized)
                c = static_cast<wchar_t>(std::towlower(c));
            std::string encoded;
            uint8_t digest[32];
            if (!WideToUtf8(normalized, encoded) || !Sha256(ByteView(encoded.data(), encoded.size()), digest)) {
                CloseHandle(lease);
                Release();
                return false;
            }
            std::wstring jobName = L"Local\\GlideAholicSlot-";
            for (uint8_t byte : digest) {
                jobName += L"0123456789abcdef"[byte >> 4];
                jobName += L"0123456789abcdef"[byte & 15];
            }
            HANDLE previous = OpenJobObjectW(JOB_OBJECT_QUERY, FALSE, jobName.c_str());
            if (previous) {
                JOBOBJECT_BASIC_ACCOUNTING_INFORMATION accounting = {};
                const bool queried = QueryInformationJobObject(previous, JobObjectBasicAccountingInformation,
                                                               &accounting, sizeof(accounting), nullptr) != 0;
                CloseHandle(previous);
                if (!queried) {
                    CloseHandle(lease);
                    Release();
                    return false;
                }
                // Even an empty still-open job belongs to an earlier owner. Never attach to it.
                CloseHandle(lease);
                CloseHandle(directories_.back());
                directories_.pop_back();
                continue;
            }
            if (GetLastError() != ERROR_FILE_NOT_FOUND) {
                CloseHandle(lease);
                Release();
                return false;
            }
            BY_HANDLE_FILE_INFORMATION info = {};
            if (!GetFileInformationByHandle(lease, &info) || info.nNumberOfLinks != 1 ||
                (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
                CloseHandle(lease);
                Release();
                return false;
            }
            LARGE_INTEGER size = {};
            bool matches = false;
            if (!GetFileSizeEx(lease, &size)) {
                CloseHandle(lease);
                Release();
                return false;
            }
            if (size.QuadPart == 64 && !identity.empty()) {
                char stored[64];
                DWORD read = 0;
                matches = ReadFile(lease, stored, sizeof(stored), &read, nullptr) && read == sizeof(stored) &&
                          !std::memcmp(stored, identity.data(), sizeof(stored));
            } else if (!size.QuadPart) {
                matches = identity.empty();
                if (!identity.empty()) {
                    // Unknown populated directories must never be adopted by a different installation.
                    WIN32_FIND_DATAW entry = {};
                    HANDLE find = FindFirstFileW((path + L"\\*").c_str(), &entry);
                    bool empty = find != INVALID_HANDLE_VALUE;
                    if (find != INVALID_HANDLE_VALUE) {
                        do {
                            if (std::wcscmp(entry.cFileName, L".") && std::wcscmp(entry.cFileName, L"..") &&
                                _wcsicmp(entry.cFileName, L".slot.lock"))
                                empty = false;
                        } while (FindNextFileW(find, &entry));
                        if (GetLastError() != ERROR_NO_MORE_FILES)
                            empty = false;
                        FindClose(find);
                    }
                    if (empty) {
                        DWORD written = 0;
                        if (!WriteFile(lease, identity.data(), 64, &written, nullptr) || written != 64 ||
                            !FlushFileBuffers(lease)) {
                            CloseHandle(lease);
                            Release();
                            return false;
                        }
                        matches = true;
                    }
                }
            }
            if (matches) {
                reservation_ = lease;
                directory_ = path;
                jobName_ = jobName;
                number_ = slot;
                error.clear();
                return true;
            }
            CloseHandle(lease);
            CloseHandle(directories_.back());
            directories_.pop_back();
            continue;
        }
        const DWORD failure = GetLastError();
        CloseHandle(directories_.back());
        directories_.pop_back();
        if (failure != ERROR_SHARING_VIOLATION) {
            Release();
            return false;
        }
    }
    Release();
    return false;
}
bool RuntimeSlot::CopyResource(const std::wstring& source, const std::wstring& relative, std::string& error) {
    error = "Cannot copy resource into private runtime";
    if (reservation_ == INVALID_HANDLE_VALUE || !RuntimeRelativePath(relative) ||
        source.find(L'\0') != std::wstring::npos)
        return false;
    for (size_t at = 0; at < relative.size(); ++at)
        if (relative[at] == L'\\' && !HoldDirectory(directory_ + L"\\" + relative.substr(0, at), true))
            return false;
    const std::wstring destination = directory_ + L"\\" + relative;
    // Copy to a fresh sibling, then replace the directory entry. Never open an existing destination for writing.
    const std::wstring temporary =
        destination + L".stage-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    Handle input(CreateFileW(source.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                             FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr));
    BY_HANDLE_FILE_INFORMATION info = {};
    if (input.Get() == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(input.Get(), &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return false;
    bool copied = false;
    {
        Handle output(
            CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
        if (output.Get() == INVALID_HANDLE_VALUE)
            return false;
        uint8_t bytes[65536];
        DWORD count = 0;
        while (ReadFile(input.Get(), bytes, sizeof(bytes), &count, nullptr)) {
            if (!count) {
                copied = FlushFileBuffers(output.Get()) != 0;
                break;
            }
            DWORD written = 0;
            if (!WriteFile(output.Get(), bytes, count, &written, nullptr) || written != count)
                break;
        }
    }
    if (copied)
        copied = MoveFileExW(temporary.c_str(), destination.c_str(),
                             MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
    if (!copied) {
        DeleteFileW(temporary.c_str());
        return false;
    }
    error.clear();
    return true;
}
bool RuntimeSlot::PrepareUserData(std::string& error) {
    error = "Cannot prepare private Documents or Local.dat";
    if (reservation_ == INVALID_HANDLE_VALUE || !HoldDirectory(directory_ + L"\\Documents", true))
        return false;
    const std::wstring local = directory_ + L"\\Local.dat";
    const DWORD attributes = GetFileAttributesW(local.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        if (GetLastError() != ERROR_FILE_NOT_FOUND)
            return false;
        error.clear();
        return true; // The native archive API creates a valid archive, not an empty placeholder.
    }
    Handle file(CreateFileW(local.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                            OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
    BY_HANDLE_FILE_INFORMATION info = {};
    if (file.Get() == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(file.Get(), &info) ||
        info.nNumberOfLinks != 1 || (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return false;
    error.clear();
    return true;
}
} // namespace glide
