#include "glide/local_archive.h"
#include "glide/files.h"
#include <Windows.h>
#include <cstring>

namespace glide {
bool CreateLocalArchive(const std::wstring& templateFile, const std::wstring& output, std::string& error) {
    error = "Cannot initialize private local archive";
    const auto attributes = GetFileAttributesW(output.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        if (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
        error.clear();
        return true;
    }
    if (GetLastError() != ERROR_FILE_NOT_FOUND)
        return false;
    Buffer bytes;
    if (!ReadFileBytes(templateFile, 896, bytes) || bytes.Size() != 896 || std::memcmp(bytes.Data() + 1, "AN\x1a", 3) ||
        std::memcmp(bytes.Data() + 512, "Mft\x1a", 4)) {
        error = "Missing or invalid local archive template; prepare this installation first";
        return false;
    }
    if (!WriteFileAtomic(output, bytes.View(), false)) {
        const auto current = GetFileAttributesW(output.c_str());
        if (current == INVALID_FILE_ATTRIBUTES || (current & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return false;
    }
    error.clear();
    return true;
}
} // namespace glide
