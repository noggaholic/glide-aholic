#include "glide/archive.h"
#include <Windows.h>
#include <io.h>
#include <share.h>
#include <map>
#include <algorithm>

namespace glide {
namespace {
constexpr uint32_t ChunkBytes = 1048576;
struct Search {
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~Search() {
        if (handle != INVALID_HANDLE_VALUE)
            FindClose(handle);
    }
};
bool Number(const std::wstring& text, uint64_t& value) {
    if (text.empty() || (text.size() > 1 && text[0] == L'0'))
        return false;
    value = 0;
    for (wchar_t ch : text) {
        if (ch < L'0' || ch > L'9')
            return false;
        const unsigned digit = ch - L'0';
        if (value > (UINT64_MAX - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    return true;
}
bool ReadAt(FILE* file, uint64_t offset, void* data, size_t size) {
    return offset <= INT64_MAX && !_fseeki64(file, static_cast<int64_t>(offset), SEEK_SET) &&
           std::fread(data, 1, size, file) == size;
}
} // namespace

std::wstring ArchiveReader::ChunkPath(uint64_t index) const {
    return path_ + L"/" + std::to_wstring(index / 100) + L"-1-100/" + std::to_wstring(index) + L".dat";
}
void ArchiveReader::Close() {
    if (file_)
        std::fclose(file_);
    file_ = nullptr;
    path_.clear();
    chunks_.clear();
    size_ = 0;
}
bool ArchiveReader::Open(const std::wstring& path, bool allowWriter) {
    Close();
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        FILE* file = nullptr;
        if (allowWriter)
            file = _wfsopen(path.c_str(), L"rb", _SH_DENYNO);
        else if (_wfopen_s(&file, path.c_str(), L"rb"))
            return false;
        if (!file)
            return false;
        const int64_t length = _filelengthi64(_fileno(file));
        if (length < 40) {
            std::fclose(file);
            return false;
        }
        file_ = file;
        size_ = static_cast<uint64_t>(length);
        return true;
    }

    // Preserve the legacy 1 MiB logical spacing, including unwritten chunk tails.
    // Enumerate all groups so a missing chunk cannot truncate the apparent archive.
    std::map<uint64_t, uint32_t> sizes;
    WIN32_FIND_DATAW group{};
    Search groups;
    groups.handle = FindFirstFileW((path + L"/*-1-100").c_str(), &group);
    if (groups.handle == INVALID_HANDLE_VALUE)
        return false;
    do {
        const std::wstring name(group.cFileName);
        uint64_t groupIndex = 0;
        if (name.size() <= 6 || name.substr(name.size() - 6) != L"-1-100" ||
            !Number(name.substr(0, name.size() - 6), groupIndex) ||
            !(group.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
            (group.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
        WIN32_FIND_DATAW part{};
        Search parts;
        parts.handle = FindFirstFileW((path + L"/" + name + L"/*.dat").c_str(), &part);
        if (parts.handle == INVALID_HANDLE_VALUE)
            return false;
        do {
            const std::wstring filename(part.cFileName);
            uint64_t index = 0;
            if (filename.size() <= 4 || filename.substr(filename.size() - 4) != L".dat" ||
                !Number(filename.substr(0, filename.size() - 4), index) || index / 100 != groupIndex ||
                index >= UINT64_MAX / ChunkBytes ||
                (part.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) ||
                part.nFileSizeHigh || !part.nFileSizeLow || part.nFileSizeLow > ChunkBytes ||
                !sizes.emplace(index, part.nFileSizeLow).second)
                return false;
        } while (FindNextFileW(parts.handle, &part));
        if (GetLastError() != ERROR_NO_MORE_FILES)
            return false;
    } while (FindNextFileW(groups.handle, &group));
    if (GetLastError() != ERROR_NO_MORE_FILES || sizes.empty())
        return false;
    uint64_t expected = 0;
    for (const auto& entry : sizes)
        if (entry.first != expected++)
            return false;
    const uint64_t length = sizes.rbegin()->first * ChunkBytes + sizes.rbegin()->second;
    if (length < 40)
        return false;
    for (const auto& entry : sizes)
        chunks_.push_back(entry.second);
    path_ = path;
    size_ = length;
    return true;
}
bool ArchiveReader::Read(uint64_t offset, void* data, size_t size) {
    if (!size_ || (!data && size) || offset > size_ || size > size_ - offset)
        return false;
    if (!size)
        return true;
    if (file_)
        return ReadAt(file_, offset, data, size);
    auto* output = static_cast<uint8_t*>(data);
    while (size) {
        const uint64_t index = offset / ChunkBytes;
        const uint32_t inside = static_cast<uint32_t>(offset % ChunkBytes);
        const size_t wanted = (std::min)(size, static_cast<size_t>(ChunkBytes - inside));
        if (inside > chunks_[static_cast<size_t>(index)] || wanted > chunks_[static_cast<size_t>(index)] - inside)
            return false;
        FILE* file = nullptr;
        if (_wfopen_s(&file, ChunkPath(index).c_str(), L"rb") || !file)
            return false;
        const bool ok = ReadAt(file, inside, output, wanted);
        std::fclose(file);
        if (!ok)
            return false;
        offset += wanted;
        output += wanted;
        size -= wanted;
    }
    return true;
}
} // namespace glide
