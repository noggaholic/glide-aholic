#include "glide/files.h"
#include "glide/crypto.h"
#include <Windows.h>
#include <climits>
#include <utility>

namespace glide {
namespace {
class FileHandle {
  public:
    explicit FileHandle(HANDLE handle) : handle_(handle) {}
    ~FileHandle() {
        Close();
    }
    void Close() {
        if (handle_ != INVALID_HANDLE_VALUE) {
            CloseHandle(handle_);
            handle_ = INVALID_HANDLE_VALUE;
        }
    }
    HANDLE Get() const {
        return handle_;
    }

  private:
    HANDLE handle_;
};
} // namespace
bool EnsureDirectory(const std::wstring& path) {
    if (path.empty())
        return false;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES)
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    if (CreateDirectoryW(path.c_str(), nullptr))
        return true;
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        const DWORD current = GetFileAttributesW(path.c_str());
        return current != INVALID_FILE_ATTRIBUTES && (current & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    const size_t slash = path.find_last_of(L"/\\");
    if (slash == std::wstring::npos || slash == 0 || !EnsureDirectory(path.substr(0, slash)))
        return false;
    if (CreateDirectoryW(path.c_str(), nullptr))
        return true;
    const DWORD current = GetFileAttributesW(path.c_str());
    return current != INVALID_FILE_ATTRIBUTES && (current & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
bool ReadFileBytes(const std::wstring& path, size_t limit, Buffer& out) {
    FileHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER length;
    if (!GetFileSizeEx(file.Get(), &length) || length.QuadPart < 0 || static_cast<uint64_t>(length.QuadPart) > limit)
        return false;
    Buffer data;
    size_t remaining = static_cast<size_t>(length.QuadPart);
    if (!data.Reserve(remaining))
        return false;
    uint8_t chunk[65536];
    while (remaining) {
        const DWORD want = static_cast<DWORD>(remaining < sizeof(chunk) ? remaining : sizeof(chunk));
        DWORD got = 0;
        if (!ReadFile(file.Get(), chunk, want, &got, nullptr) || got != want || !data.Append(ByteView(chunk, got)))
            return false;
        remaining -= got;
    }
    out = std::move(data);
    return true;
}
bool WriteFileAtomic(const std::wstring& path, ByteView data, bool replace) {
    if (path.empty() || !data.Valid())
        return false;
    uint8_t random[16];
    if (!RandomBytes(random, sizeof(random)))
        return false;
    std::wstring temporary = path + L".";
    const wchar_t* hex = L"0123456789abcdef";
    for (uint8_t b : random) {
        temporary += hex[b >> 4];
        temporary += hex[b & 15];
    }
    temporary += L".tmp";
    FileHandle file(
        CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.Get() == INVALID_HANDLE_VALUE)
        return false;
    bool ok = true;
    size_t at = 0;
    while (at < data.size) {
        const DWORD want = static_cast<DWORD>(data.size - at > 65536 ? 65536 : data.size - at);
        DWORD written = 0;
        if (!WriteFile(file.Get(), data.data + at, want, &written, nullptr) || written != want) {
            ok = false;
            break;
        }
        at += written;
    }
    if (ok)
        ok = FlushFileBuffers(file.Get()) != FALSE;
    file.Close();
    if (ok)
        ok = MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_WRITE_THROUGH | (replace ? MOVEFILE_REPLACE_EXISTING : 0)) != FALSE;
    if (!ok)
        DeleteFileW(temporary.c_str());
    return ok;
}
bool Utf8ToWide(const std::string& input, std::wstring& out) {
    if (input.empty()) {
        out.clear();
        return true;
    }
    if (input.size() > INT_MAX || input.find('\0') != std::string::npos)
        return false;
    const int size =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), nullptr, 0);
    if (size <= 0)
        return false;
    std::wstring result(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), &result[0],
                            size) != size)
        return false;
    out = std::move(result);
    return true;
}
bool WideToUtf8(const std::wstring& input, std::string& out) {
    if (input.empty()) {
        out.clear();
        return true;
    }
    if (input.size() > INT_MAX || input.find(L'\0') != std::wstring::npos)
        return false;
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()),
                                         nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return false;
    std::string result(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>(input.size()), &result[0],
                            size, nullptr, nullptr) != size)
        return false;
    out = std::move(result);
    return true;
}
DirectoryLock::~DirectoryLock() {
    Release();
}
void DirectoryLock::Release() {
    if (handle_) {
        CloseHandle(handle_);
        handle_ = nullptr;
    }
}
bool DirectoryLock::Acquire(const std::wstring& directory) {
    if (Held())
        return false;
    HANDLE file = CreateFileW((directory + L"/.glide-write.lock").c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    handle_ = file;
    return true;
}
} // namespace glide
