#include "resources.h"
#include "glide/builds.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <cwctype>

namespace glide {
bool PreparedRuntime::Prepare(const std::wstring& sourceExe, const std::wstring& manifestPath,
                              const std::wstring& runtimeRoot, std::unique_ptr<PreparedRuntime>& out,
                              std::string& error, const std::wstring& contentDirectory) {
    Buffer data;
    error = "Cannot read runtime resource manifest";
    if (!ReadFileBytes(manifestPath, 1024 * 1024, data) || !data.Size())
        return false;
    std::unique_ptr<PreparedRuntime> prepared(new PreparedRuntime);
    if (!RuntimeManifest::Parse(std::string(reinterpret_cast<const char*>(data.Data()), data.Size()),
                                prepared->manifest_, error))
        return false;
    const size_t slash = sourceExe.find_last_of(L"/\\");
    if (slash == std::wstring::npos) {
        error = "Source executable requires an absolute path";
        return false;
    }
    const std::wstring sourceRoot = sourceExe.substr(0, slash);
    prepared->archive_ = sourceRoot + L"\\" + prepared->manifest_.archive;
    std::vector<std::wstring> originals{sourceExe, prepared->archive_};
    for (const auto& resource : prepared->manifest_.resources) {
        const std::wstring source = sourceRoot + L"\\" + resource;
        const DWORD attributes = GetFileAttributesW(source.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            std::string name;
            WideToUtf8(resource, name);
            error = "Missing or invalid runtime resource: " + name;
            return false;
        }
        originals.push_back(source);
    }
    if (!prepared->manifest_.packages.empty() &&
        (contentDirectory.size() < 3 || contentDirectory[1] != L':' || contentDirectory[2] != L'\\')) {
        error = "Packaged runtime resources require an absolute content directory";
        return false;
    }
    for (const auto& package : prepared->manifest_.packages)
        originals.push_back(contentDirectory + L"\\" + package.source);
    if (!prepared->originals_.Acquire(originals, error))
        return false;
    for (const auto& package : prepared->manifest_.packages)
        if (!package.Verify(contentDirectory + L"\\" + package.source, error))
            return false;
    PeImage image;
    const BuildInfo* build = nullptr;
    if (!PeImage::FromFile(sourceExe.c_str(), image) ||
        DetectBuild(image, prepared->manifest_.build, build) != BuildDetection::Found) {
        error = "Executable does not match runtime manifest build";
        return false;
    }
    // Bind reusable private data to the installation, manifest and exact executable bytes.
    // Changed resource lists therefore cannot leave old libraries in a newly prepared slot.
    wchar_t full[32768];
    const DWORD length = GetFullPathNameW(sourceExe.c_str(), 32768, full, nullptr);
    if (!length || length >= 32768) {
        error = "Cannot normalize installation path";
        return false;
    }
    std::wstring normalized(full);
    for (auto& c : normalized)
        c = static_cast<wchar_t>(std::towlower(c));
    std::string path;
    Buffer executable, identityBytes;
    uint8_t digest[32];
    if (!WideToUtf8(normalized, path) || !ReadFileBytes(sourceExe, 512 * 1024 * 1024, executable) ||
        !Sha256(executable.View(), digest) || !identityBytes.Append(ByteView(path.data(), path.size())) ||
        !identityBytes.U8(0) || !identityBytes.Append(ByteView(digest, sizeof(digest))) ||
        !identityBytes.Append(data.View()) || !Sha256(identityBytes.View(), digest)) {
        error = "Cannot identify runtime source";
        return false;
    }
    std::string identity;
    for (uint8_t byte : digest) {
        identity += "0123456789abcdef"[byte >> 4];
        identity += "0123456789abcdef"[byte & 15];
    }
    if (!prepared->slot_.Reserve(runtimeRoot, build->build, error, identity) ||
        !prepared->slot_.CopyResource(sourceExe, prepared->manifest_.executable, error))
        return false;
    for (const auto& resource : prepared->manifest_.resources)
        if (!prepared->slot_.CopyResource(sourceRoot + L"\\" + resource, resource, error))
            return false;
    for (const auto& package : prepared->manifest_.packages) {
        if (!prepared->slot_.CopyResource(contentDirectory + L"\\" + package.source, package.destination, error) ||
            !package.Verify(prepared->slot_.Directory() + L"\\" + package.destination, error))
            return false;
    }
    if (build->build == 167409 &&
        !prepared->slot_.CopyResource(prepared->archive_ + L"\\0-1-100\\0.dat", L"MainHeader0.dat", error)) {
        error = "Cannot stage the original China archive header privately";
        return false;
    }
    if (!prepared->slot_.PrepareUserData(error))
        return false;
    prepared->executable_ = prepared->slot_.Directory() + L"\\" + prepared->manifest_.executable;
    out = std::move(prepared);
    error.clear();
    return true;
}
} // namespace glide
