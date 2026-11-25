#include "glide/installation_data.h"
#include "glide/crypto.h"
#include "glide/files.h"
#include "glide/runtime_manifest.h"
#include <Windows.h>
#include <algorithm>
#include <cstring>
#include <set>

namespace glide {
namespace {
constexpr size_t FooterSize = 72, MaximumIndex = 16 * 1024 * 1024, MaximumFile = 512 * 1024 * 1024;
const char Magic[8] = {'G', 'L', 'I', 'D', 'A', 'T', 'A', '1'};
uint32_t U32(const uint8_t* p) {
    return p[0] | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}
uint64_t U64(const uint8_t* p) {
    return U32(p) | (uint64_t(U32(p + 4)) << 32);
}
bool Name(const std::string& name) {
    // Keep original filenames, but reject Windows path aliases.
    if (name.empty() || name.size() > 240 || name.find('\\') != std::string::npos)
        return false;
    for (char c : name)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '.' ||
              c == '-' || c == '_'))
            return false;
    std::wstring path(name.begin(), name.end());
    std::replace(path.begin(), path.end(), L'/', L'\\');
    return RuntimeRelativePath(path);
}
std::string NameKey(std::string value) {
    for (auto& c : value)
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
    return value;
}
bool PlainPath(const std::wstring& path) {
    // Check all existing ancestors, including the output root. Checking only
    // the final file would allow a directory junction to redirect extraction.
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\' && path[i] != L'/')
            continue;
        const auto part = path.substr(0, i);
        if (part.empty() || part.back() == L':')
            continue;
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
        if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND &&
            GetLastError() != ERROR_PATH_NOT_FOUND)
            return false;
    }
    return true;
}
struct Output {
    HANDLE handle = INVALID_HANDLE_VALUE;
    std::wstring path;
    bool owned = false;
    ~Output() {
        Close();
        if (owned && !path.empty())
            DeleteFileW(path.c_str());
    }
    void Close() {
        if (handle != INVALID_HANDLE_VALUE)
            CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;
    }
    bool Write(ByteView bytes) {
        for (size_t at = 0; at < bytes.size;) {
            DWORD count = static_cast<DWORD>((std::min)(bytes.size - at, size_t(65536))), written = 0;
            if (!WriteFile(handle, bytes.data + at, count, &written, nullptr) || written != count)
                return false;
            at += count;
        }
        return true;
    }
};
bool Payload(Output& file, uint64_t originalSize, unsigned build, const std::vector<InstallationDataInput>& inputs) {
    Buffer index;
    if (!index.U32(static_cast<uint32_t>(inputs.size())))
        return false;
    uint64_t offset = originalSize;
    for (const auto& input : inputs) {
        Buffer bytes;
        uint8_t digest[32];
        if (!ReadFileBytes(input.source, MaximumFile, bytes) || !Sha256(bytes.View(), digest) ||
            !index.U32(static_cast<uint32_t>(input.name.size())) || !index.U64(offset) ||
            !index.U32(static_cast<uint32_t>(bytes.Size())) || !index.Append(ByteView(digest, 32)) ||
            !index.Append(ByteView(input.name.data(), input.name.size())) || index.Size() > MaximumIndex ||
            !file.Write(bytes.View()) || bytes.Size() > UINT64_MAX - offset)
            return false;
        offset += bytes.Size();
    }
    uint8_t digest[32];
    Buffer footer;
    if (!Sha256(index.View(), digest) || !footer.Append(ByteView(Magic, 8)) || !footer.U32(1) || !footer.U32(build) ||
        !footer.U64(originalSize) || !footer.U64(offset) || !footer.U64(index.Size()) ||
        !footer.Append(ByteView(digest, 32)) || !file.Write(index.View()) || !file.Write(footer.View()) ||
        !FlushFileBuffers(file.handle))
        return false;
    return true;
}
bool Seek(HANDLE handle, uint64_t offset) {
    LARGE_INTEGER at;
    at.QuadPart = static_cast<LONGLONG>(offset);
    return offset <= INT64_MAX && SetFilePointerEx(handle, at, nullptr, FILE_BEGIN) != FALSE;
}
bool ReadAt(HANDLE handle, uint64_t offset, void* bytes, DWORD size) {
    DWORD read = 0;
    return Seek(handle, offset) && ReadFile(handle, bytes, size, &read, nullptr) && read == size;
}
bool Identity(HANDLE handle, unsigned build, uint64_t originalSize, Buffer& out) {
    BY_HANDLE_FILE_INFORMATION info{};
    uint8_t header[40], tail[72], a[32], b[32];
    if (originalSize < sizeof(tail) || !GetFileInformationByHandle(handle, &info) ||
        !ReadAt(handle, 0, header, sizeof(header)) ||
        !ReadAt(handle, originalSize - sizeof(tail), tail, sizeof(tail)) ||
        !Sha256(ByteView(header, sizeof(header)), a) || !Sha256(ByteView(tail, sizeof(tail)), b))
        return false;
    return out.Append(ByteView("GLIDAPP1", 8)) && out.U32(build) && out.U64(originalSize) &&
           out.U32(info.dwVolumeSerialNumber) && out.U32(info.nFileIndexHigh) && out.U32(info.nFileIndexLow) &&
           out.Append(ByteView(a, 32)) && out.Append(ByteView(b, 32));
}
bool OpenPending(const std::wstring& path, unsigned build, Output& target, ArchiveReader& pending,
                 uint64_t& originalSize, std::string& error) {
    error = "Invalid append recovery record; archive is preserved";
    if (!PlainPath(path) || !PlainPath(path + L".glide-append") || !pending.Open(path + L".glide-append") ||
        pending.Size() < 96)
        return false;
    uint8_t header[96];
    if (!pending.Read(0, header, sizeof(header)) || std::memcmp(header, "GLIDAPP1", 8) || U32(header + 8) != build)
        return false;
    originalSize = U64(header + 12);
    target.handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    if (target.handle == INVALID_HANDLE_VALUE)
        return false;
    LARGE_INTEGER length;
    Buffer identity;
    return GetFileSizeEx(target.handle, &length) && originalSize <= uint64_t(length.QuadPart) &&
           uint64_t(length.QuadPart) - originalSize <= pending.Size() - 96 &&
           Identity(target.handle, build, originalSize, identity) && identity.Size() == sizeof(header) &&
           !std::memcmp(header, identity.Data(), sizeof(header));
}
} // namespace

bool InstallationData::Open(const std::wstring& path, unsigned build, std::string& error) {
    return OpenImpl(path, build, error, false);
}
bool InstallationData::OpenImpl(const std::wstring& path, unsigned build, std::string& error, bool allowWriter) {
    entries_.clear();
    originalSize_ = 0;
    baseOffset_ = 0;
    build_ = 0;
    archive_.Close();
    error = "Missing or invalid Glide installation data";
    std::wstring package = path;
    const auto attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        ArchiveReader native;
        if (!native.Open(path) || !native.Chunked())
            return false;
        baseOffset_ = native.Size();
        package += L"/glide-data.bin";
    }
    if (!archive_.Open(package, allowWriter) || archive_.Size() < FooterSize + 4 ||
        archive_.Size() > UINT64_MAX - baseOffset_)
        return false;
    uint8_t footer[FooterSize], digest[32];
    const auto end = archive_.Size() + baseOffset_ - FooterSize;
    if (!archive_.Read(end - baseOffset_, footer, sizeof(footer)) || std::memcmp(footer, Magic, 8) ||
        U32(footer + 8) != 1 || !U32(footer + 12) || (build && U32(footer + 12) != build))
        return false;
    const auto original = U64(footer + 16), indexAt = U64(footer + 24), indexSize = U64(footer + 32);
    if (original < 40 || original > indexAt || indexAt < baseOffset_ || indexAt > end ||
        (baseOffset_ && original != baseOffset_) || indexSize != end - indexAt || indexSize < 4 ||
        indexSize > MaximumIndex)
        return false;
    Buffer index;
    if (!index.Resize(static_cast<size_t>(indexSize)) ||
        !archive_.Read(indexAt - baseOffset_, index.Data(), index.Size()) || !Sha256(index.View(), digest) ||
        std::memcmp(digest, footer + 40, 32))
        return false;
    const auto count = U32(index.Data());
    if (!count || count > 65536)
        return false;
    size_t at = 4;
    uint64_t next = original;
    std::map<std::string, Entry> entries;
    std::set<std::string> names;
    for (unsigned i = 0; i < count; ++i) {
        if (index.Size() - at < 48)
            return false;
        const auto length = U32(index.Data() + at);
        Entry entry;
        entry.offset = U64(index.Data() + at + 4);
        entry.size = U32(index.Data() + at + 12);
        std::memcpy(entry.hash.data(), index.Data() + at + 16, 32);
        at += 48;
        if (!length || length > index.Size() - at || entry.offset != next || entry.offset > indexAt ||
            entry.size > indexAt - entry.offset || entry.size > MaximumFile)
            return false;
        std::string name(reinterpret_cast<const char*>(index.Data() + at), length);
        if (!Name(name) || !names.insert(NameKey(name)).second || !entries.emplace(name, entry).second)
            return false;
        at += length;
        next += entry.size;
    }
    if (at != index.Size() || next != indexAt)
        return false;
    entries_ = std::move(entries);
    originalSize_ = original;
    build_ = U32(footer + 12);
    error.clear();
    return true;
}

bool InstallationData::Read(const std::string& name, Buffer& out, std::string& error) {
    error = "Missing or changed installation data: " + name;
    const auto found = entries_.find(name);
    if (found == entries_.end())
        return false;
    const auto& entry = found->second;
    Buffer bytes;
    uint8_t digest[32];
    if (!bytes.Resize(entry.size) || !archive_.Read(entry.offset - baseOffset_, bytes.Data(), bytes.Size()) ||
        !Sha256(bytes.View(), digest) || std::memcmp(digest, entry.hash.data(), 32))
        return false;
    out = std::move(bytes);
    error.clear();
    return true;
}

bool InstallationData::Verify(std::string& error) {
    if (entries_.empty()) {
        error = "Installation data is not open";
        return false;
    }
    for (const auto& entry : entries_) {
        Buffer bytes;
        if (!Read(entry.first, bytes, error))
            return false;
    }
    error.clear();
    return true;
}

bool InstallationData::Extract(const std::wstring& directory, std::string& error, const std::string& prefix) {
    error = "Invalid installation data output directory";
    wchar_t absolute[32768];
    const DWORD length = GetFullPathNameW(directory.c_str(), 32768, absolute, nullptr);
    if (!length || length >= 32768)
        return false;
    std::wstring root(absolute, length);
    std::replace(root.begin(), root.end(), L'/', L'\\');
    if (!PlainPath(root) || !Verify(error))
        return false;
    if (!prefix.empty() && (prefix.back() != '/' || !Name(prefix.substr(0, prefix.size() - 1)))) {
        error = "Invalid installation data prefix";
        return false;
    }
    size_t selected = 0;
    for (const auto& entry : entries_)
        if (entry.first.compare(0, prefix.size(), prefix) == 0)
            ++selected;
    if (!selected) {
        error = "Installation data directory is missing: " + prefix;
        return false;
    }
    // Check every existing destination before writing the first file.
    for (const auto& entry : entries_) {
        if (entry.first.compare(0, prefix.size(), prefix) != 0)
            continue;
        const auto relative = entry.first.substr(prefix.size());
        std::wstring name(relative.begin(), relative.end());
        std::replace(name.begin(), name.end(), L'/', L'\\');
        const auto path = root + L"\\" + name;
        error = "Existing output differs or uses a link: " + entry.first;
        if (!PlainPath(path))
            return false;
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
            continue;
        Buffer bytes;
        uint8_t digest[32];
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) || !ReadFileBytes(path, MaximumFile, bytes) ||
            bytes.Size() != entry.second.size || !Sha256(bytes.View(), digest) ||
            std::memcmp(digest, entry.second.hash.data(), 32))
            return false;
    }
    for (const auto& entry : entries_) {
        if (entry.first.compare(0, prefix.size(), prefix) != 0)
            continue;
        const auto relative = entry.first.substr(prefix.size());
        std::wstring name(relative.begin(), relative.end());
        std::replace(name.begin(), name.end(), L'/', L'\\');
        const auto path = root + L"\\" + name;
        if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
            continue;
        Buffer bytes;
        if (!Read(entry.first, bytes, error))
            return false;
        error = "Cannot write installation data output: " + entry.first;
        const auto parent = path.substr(0, path.find_last_of(L'\\'));
        if (!PlainPath(parent) || !EnsureDirectory(parent) || !PlainPath(parent) ||
            !WriteFileAtomic(path, bytes.View(), false))
            return false;
    }
    error.clear();
    return true;
}

bool InstallationData::Create(const std::wstring& original, const std::wstring& output, unsigned build,
                              const std::vector<InstallationDataInput>& inputs, std::string& error) {
    error = "Cannot create installation data; existing output is preserved";
    if (!build || inputs.empty() || inputs.size() > 65536 ||
        GetFileAttributesW(output.c_str()) != INVALID_FILE_ATTRIBUTES)
        return false;
    std::set<std::string> names;
    for (const auto& input : inputs)
        if (!Name(input.name) || !names.insert(NameKey(input.name)).second)
            return false;
    ArchiveReader source;
    if (!source.Open(original))
        return false;
    uint64_t required = source.Size() + MaximumIndex + FooterSize;
    if (required < source.Size())
        return false;
    for (const auto& input : inputs) {
        WIN32_FILE_ATTRIBUTE_DATA attributes{};
        if (!GetFileAttributesExW(input.source.c_str(), GetFileExInfoStandard, &attributes) ||
            (attributes.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return false;
        const auto size = (uint64_t(attributes.nFileSizeHigh) << 32) | attributes.nFileSizeLow;
        if (size > MaximumFile || size > UINT64_MAX - required)
            return false;
        required += size;
    }
    const auto parent = output.substr(0, output.find_last_of(L"/\\"));
    ULARGE_INTEGER available{};
    if (!GetDiskFreeSpaceExW(parent.c_str(), &available, nullptr, nullptr) || available.QuadPart < required) {
        error = "Not enough free space to copy the archive and add installation data";
        return false;
    }
    // Rebuilding a package must start from the original archive, not nest packages.
    uint8_t tail[FooterSize];
    if (source.Size() >= FooterSize && source.Read(source.Size() - FooterSize, tail, sizeof(tail)) &&
        !std::memcmp(tail, Magic, 8)) {
        error = "Archive already contains installation data";
        return false;
    }
    uint8_t random[16];
    if (!RandomBytes(random, sizeof(random)))
        return false;
    Output file;
    file.path = output + L".";
    for (auto byte : random) {
        file.path += L"0123456789abcdef"[byte >> 4];
        file.path += L"0123456789abcdef"[byte & 15];
    }
    file.path += L".tmp";
    file.handle = CreateFileW(file.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file.handle == INVALID_HANDLE_VALUE)
        return false;
    file.owned = true;
    Buffer chunk;
    if (!chunk.Resize(1024 * 1024))
        return false;
    for (uint64_t at = 0; at < source.Size();) {
        const auto count = static_cast<size_t>((std::min)(uint64_t(chunk.Size()), source.Size() - at));
        if (!source.Read(at, chunk.Data(), count) || !file.Write(ByteView(chunk.Data(), count)))
            return false;
        at += count;
    }
    if (!Payload(file, source.Size(), build, inputs))
        return false;
    file.Close();
    {
        InstallationData checked;
        if (!checked.Open(file.path, build, error) || !checked.Verify(error))
            return false;
    }
    if (!MoveFileExW(file.path.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot publish completed archive; existing output is preserved";
        return false;
    }
    file.path.clear();
    error.clear();
    return true;
}

bool InstallationData::StageAppend(const std::wstring& archive, unsigned build,
                                   const std::vector<InstallationDataInput>& inputs, std::string& error, bool update) {
    error = "Cannot stage archive append; existing files are preserved";
    if (!build || inputs.empty() || inputs.size() > 65536 || !PlainPath(archive))
        return false;
    std::set<std::string> names;
    uint64_t required = MaximumIndex + FooterSize + 96;
    for (const auto& input : inputs) {
        WIN32_FILE_ATTRIBUTE_DATA info{};
        if (!Name(input.name) || !names.insert(NameKey(input.name)).second ||
            !GetFileAttributesExW(input.source.c_str(), GetFileExInfoStandard, &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
            return false;
        const auto size = (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
        if (size > MaximumFile || size > UINT64_MAX - required)
            return false;
        required += size;
    }
    ULARGE_INTEGER available{};
    const auto parent = archive.substr(0, archive.find_last_of(L"/\\"));
    if (required > UINT64_MAX / 2 || !GetDiskFreeSpaceExW(parent.c_str(), &available, nullptr, nullptr) ||
        available.QuadPart < 2 * required) {
        error = "Not enough space for staged and committed data";
        return false;
    }
    Output source;
    source.handle = CreateFileW(archive.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    LARGE_INTEGER length;
    uint8_t tail[FooterSize];
    if (source.handle == INVALID_HANDLE_VALUE || !GetFileSizeEx(source.handle, &length) || length.QuadPart < 72 ||
        !ReadAt(source.handle, uint64_t(length.QuadPart) - sizeof(tail), tail, sizeof(tail)) ||
        (!update && !std::memcmp(tail, Magic, 8)))
        return false;
    Buffer identity;
    if (!Identity(source.handle, build, uint64_t(length.QuadPart), identity))
        return false;
    Output staged;
    staged.path = archive + L".glide-append";
    staged.handle =
        CreateFileW(staged.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (staged.handle == INVALID_HANDLE_VALUE)
        return false;
    staged.owned = true;
    if (!staged.Write(identity.View()) || !Payload(staged, uint64_t(length.QuadPart), build, inputs))
        return false;
    staged.Close();
    staged.owned = false;
    error.clear();
    return true;
}

bool InstallationData::AppendChunked(const std::wstring& archive, unsigned build,
                                     const std::vector<InstallationDataInput>& inputs, std::string& error) {
    error = "Cannot complete chunked archive; existing files are preserved";
    if (!build || inputs.empty() || inputs.size() > 65536 || !PlainPath(archive))
        return false;
    std::set<std::string> names;
    for (const auto& input : inputs)
        if (!Name(input.name) || !names.insert(NameKey(input.name)).second)
            return false;
    ArchiveReader native;
    if (!native.Open(archive) || !native.Chunked())
        return false;
    const auto output = archive + L"/glide-data.bin";
    if (GetFileAttributesW(output.c_str()) != INVALID_FILE_ATTRIBUTES)
        return false;
    Output staged;
    uint8_t random[16];
    if (!RandomBytes(random, sizeof(random)))
        return false;
    staged.path = archive + L"/glide-data-";
    for (auto byte : random) {
        staged.path += L"0123456789abcdef"[byte >> 4];
        staged.path += L"0123456789abcdef"[byte & 15];
    }
    staged.path += L".pending";
    staged.handle =
        CreateFileW(staged.path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (staged.handle == INVALID_HANDLE_VALUE)
        return false;
    staged.owned = true;
    if (!Payload(staged, native.Size(), build, inputs))
        return false;
    staged.Close();
    if (!MoveFileExW(staged.path.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH))
        return false;
    staged.path.clear();
    bool valid = false;
    {
        InstallationData checked;
        valid = checked.Open(archive, build, error) && checked.Verify(error);
    }
    if (!valid) {
        DeleteFileW(output.c_str());
        return false;
    }
    error.clear();
    return true;
}

bool InstallationData::RollbackAppend(const std::wstring& archive, unsigned build, std::string& error) {
    Output target;
    ArchiveReader pending;
    uint64_t originalSize = 0;
    if (!OpenPending(archive, build, target, pending, originalSize, error) || !Seek(target.handle, originalSize) ||
        !SetEndOfFile(target.handle) || !FlushFileBuffers(target.handle))
        return false;
    target.Close();
    pending.Close();
    if (!DeleteFileW((archive + L".glide-append").c_str())) {
        error = "Archive restored; cannot remove recovery record";
        return false;
    }
    error.clear();
    return true;
}

bool InstallationData::CommitAppend(const std::wstring& archive, unsigned build, std::string& error) {
    Output target;
    ArchiveReader pending;
    uint64_t originalSize = 0;
    if (!OpenPending(archive, build, target, pending, originalSize, error))
        return false;
    if (pending.Size() <= 96 + FooterSize) {
        error = "Staged payload is incomplete; recover before retrying";
        return false;
    }
    if (!Seek(target.handle, originalSize) || !SetEndOfFile(target.handle))
        return false;
    Buffer chunk;
    bool ok = chunk.Resize(1024 * 1024);
    for (uint64_t at = 96; ok && at < pending.Size();) {
        const auto count = static_cast<size_t>((std::min)(uint64_t(chunk.Size()), pending.Size() - at));
        ok = pending.Read(at, chunk.Data(), count) && target.Write(ByteView(chunk.Data(), count));
        at += count;
    }
    ok = ok && FlushFileBuffers(target.handle);
    if (ok) {
        InstallationData checked;
        ok = checked.OpenImpl(archive, build, error, true) && checked.Verify(error);
    }
    if (!ok) {
        const bool restored =
            Seek(target.handle, originalSize) && SetEndOfFile(target.handle) && FlushFileBuffers(target.handle);
        error = restored ? "Append failed; original archive restored, recovery record retained"
                         : "Append interrupted; recovery record retained";
        return false;
    }
    target.Close();
    pending.Close();
    if (!DeleteFileW((archive + L".glide-append").c_str())) {
        error = "Data installed; recovery record retained";
        return false;
    }
    error.clear();
    return true;
}
} // namespace glide
