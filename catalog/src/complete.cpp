#include "complete.h"
#include "glide/builds.h"
#include "glide/crypto.h"
#include "glide/dat_archive.h"
#include "glide/files.h"
#include "glide/installation_data.h"
#include "glide/kv.h"
#include "glide/runtime_manifest.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <set>

namespace {
bool Absolute(const std::wstring& value, std::wstring& out) {
    wchar_t path[32768];
    const DWORD count = GetFullPathNameW(value.c_str(), 32768, path, nullptr);
    if (!count || count >= 32768)
        return false;
    out.assign(path, count);
    return true;
}
bool Under(const std::wstring& path, const std::wstring& parent) {
    return path.size() >= parent.size() && !_wcsnicmp(path.c_str(), parent.c_str(), parent.size()) &&
           (path.size() == parent.size() || path[parent.size()] == L'\\');
}
bool Regular(const std::wstring& path) {
    // Reject junctions as well as final-file links for all supplied paths.
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\')
            continue;
        const auto part = path.substr(0, i);
        if (part.empty() || part.back() == L':')
            continue;
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            (i == path.size() && (attributes & FILE_ATTRIBUTE_DIRECTORY)))
            return false;
    }
    return true;
}
bool Destination(const std::wstring& path) {
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i != path.size() && path[i] != L'\\')
            continue;
        const auto part = path.substr(0, i);
        if (part.empty() || part.back() == L':')
            continue;
        const auto attributes = GetFileAttributesW(part.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
            return false;
    }
    return true;
}
bool Document(const std::wstring& file, glide::KvDocument& doc, std::string& error) {
    glide::Buffer bytes;
    return Regular(file) && glide::ReadFileBytes(file, 16 * 1024 * 1024, bytes) &&
           glide::KvDocument::Parse(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()), doc,
                                    error) &&
           doc.Sections().empty();
}
bool Matches(glide::ByteView bytes, unsigned size, const std::string& hash) {
    uint8_t digest[32];
    if (bytes.size != size || hash.size() != 64 || !glide::Sha256(bytes, digest))
        return false;
    for (unsigned i = 0; i < 32; ++i)
        if (hash[2 * i] != "0123456789abcdef"[digest[i] >> 4] || hash[2 * i + 1] != "0123456789abcdef"[digest[i] & 15])
            return false;
    return true;
}
bool Inputs(const std::wstring& root, unsigned build, std::vector<glide::InstallationDataInput>& out,
            std::string& error) {
    glide::KvDocument doc;
    std::string value;
    error = "Invalid installation.txt import manifest";
    if (!Document(root + L"\\installation.txt", doc, error) || doc.Count("", "format") != 1 ||
        !doc.Get("", "format", value) || value != "1" || doc.Count("", "build") != 1 || !doc.Get("", "build", value) ||
        value != std::to_string(build))
        return false;
    std::set<std::string> names;
    for (const auto& row : doc.Entries("")) {
        if (row.first == "format" || row.first == "build")
            continue;
        const auto a = row.second.find('|'), b = row.second.find('|', a == std::string::npos ? a : a + 1);
        if (row.first != "data" || a == std::string::npos || b == std::string::npos ||
            row.second.find('|', b + 1) != std::string::npos)
            return false;
        const auto name = row.second.substr(0, a);
        std::wstring relative;
        if (!glide::Utf8ToWide(name, relative))
            return false;
        std::replace(relative.begin(), relative.end(), L'/', L'\\');
        if (!glide::RuntimeRelativePath(relative) || !names.insert(name).second)
            return false;
        unsigned size = 0;
        glide::Buffer bytes;
        const auto path = root + L"\\" + relative;
        error = "Missing or changed import file: " + name;
        if (!glide::ParseUnsigned(row.second.substr(a + 1, b - a - 1), 512 * 1024 * 1024, size) || !Regular(path) ||
            !glide::ReadFileBytes(path, 512 * 1024 * 1024, bytes) ||
            !Matches(bytes.View(), size, row.second.substr(b + 1)))
            return false;
        out.push_back({name, path});
    }
    return !out.empty();
}
bool Copy(const std::wstring& source, const std::wstring& target) {
    if (!Regular(source) || !Destination(target) ||
        !glide::EnsureDirectory(target.substr(0, target.find_last_of(L'\\'))))
        return false;
    if (GetFileAttributesW(target.c_str()) == INVALID_FILE_ATTRIBUTES)
        return CopyFileW(source.c_str(), target.c_str(), TRUE) != FALSE;
    glide::Buffer a, b;
    uint8_t ah[32], bh[32];
    return Regular(target) && glide::ReadFileBytes(source, 512 * 1024 * 1024, a) &&
           glide::ReadFileBytes(target, 512 * 1024 * 1024, b) && a.Size() == b.Size() && glide::Sha256(a.View(), ah) &&
           glide::Sha256(b.View(), bh) && !std::memcmp(ah, bh, 32);
}
} // namespace
int CompleteMain(int argc, wchar_t** argv) {
    const bool recover = argc == 3 && !std::wcscmp(argv[2], L"--recover");
    const bool update = argc == 6 && !std::wcscmp(argv[5], L"--update");
    const bool inPlace =
        (argc == 5 || update) && !std::wcscmp(argv[2], L"--inputs") && !std::wcscmp(argv[4], L"--in-place");
    if (!recover && !inPlace && (argc != 6 || std::wcscmp(argv[2], L"--inputs") || std::wcscmp(argv[4], L"--out"))) {
        std::puts("Usage: glide-catalog complete <game-executable> --inputs directory [--out "
                  "new-installation-directory | --in-place [--update]]\n"
                  "Recovery: glide-catalog complete <game-executable> --recover");
        return 2;
    }
    std::wstring executable, inputsRoot, output;
    if (!Absolute(argv[1], executable) || (!recover && !Absolute(argv[3], inputsRoot)) ||
        (!recover && !inPlace && !Absolute(argv[5], output)))
        return 2;
    const auto game = executable.substr(0, executable.find_last_of(L'\\'));
    if (!recover && !inPlace &&
        (Under(output, game) || Under(game, output) || Under(inputsRoot, output) ||
         GetFileAttributesW(output.c_str()) != INVALID_FILE_ATTRIBUTES)) {
        std::puts("Choose a new output directory outside the source installation and import files");
        return 1;
    }
    const glide::BuildInfo* build = nullptr;
    glide::PeImage image;
    if (!Regular(executable) || !glide::PeImage::FromFile(executable.c_str(), image) ||
        glide::DetectBuild(image, 0, build) != glide::BuildDetection::Found || !build)
        return 1;
    std::string error;
    if (recover) {
        if (!glide::InstallationData::RollbackAppend(game + L"\\Gw2.dat", build->build, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::puts("Original archive restored");
        return 0;
    }
    std::vector<glide::InstallationDataInput> inputs;
    if (!Inputs(inputsRoot, build->build, inputs, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    glide::Buffer manifestBytes;
    glide::RuntimeManifest manifest;
    if (!glide::ReadFileBytes(L"builds/" + std::to_wstring(build->build) + L"/runtime-manifest.txt", 1024 * 1024,
                              manifestBytes) ||
        !glide::RuntimeManifest::Parse(
            std::string(reinterpret_cast<const char*>(manifestBytes.Data()), manifestBytes.Size()), manifest, error) ||
        manifest.build != build->build)
        return 1;
    if (inPlace) {
        glide::DirectoryLock lock;
        if (!lock.Acquire(game)) {
            std::fprintf(stderr, "Cannot lock installation (Windows error %lu)\n", GetLastError());
            return 1;
        }
        const auto dat = game + L"\\" + manifest.archive;
        for (const auto& resource : manifest.resources)
            if (!Regular(game + L"\\" + resource)) {
                std::puts("Original runtime components must be installed before completing this build");
                return 1;
            }
        if (GetFileAttributesW((dat + L".glide-append").c_str()) != INVALID_FILE_ATTRIBUTES &&
            !glide::InstallationData::RollbackAppend(dat, build->build, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        {
            glide::InstallationData current;
            if (current.Open(dat, build->build, error)) {
                bool matches = current.Count() == inputs.size();
                for (const auto& input : inputs) {
                    glide::Buffer expected, actual;
                    if (!current.Read(input.name, actual, error) ||
                        !glide::ReadFileBytes(input.source, 512 * 1024 * 1024, expected) ||
                        expected.Size() != actual.Size() ||
                        std::memcmp(expected.Data(), actual.Data(), actual.Size())) {
                        matches = false;
                        break;
                    }
                }
                if (matches) {
                    std::puts("Installation already contains the requested data");
                    return 0;
                }
                if (!update) {
                    std::puts("Different data already installed; preserved (use --update to add a new revision)");
                    return 1;
                }
            }
        }
        const auto attributes = GetFileAttributesW(dat.c_str());
        const bool chunked = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        const bool installed = chunked
                                   ? glide::InstallationData::AppendChunked(dat, build->build, inputs, error)
                                   : glide::InstallationData::StageAppend(dat, build->build, inputs, error, update) &&
                                         glide::InstallationData::CommitAppend(dat, build->build, error);
        if (!installed) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        std::printf("Build %u: installation data added; original archive bytes preserved\n", build->build);
        return 0;
    }
    const auto stage = output + L".pending";
    if (!Destination(stage) || !glide::EnsureDirectory(stage))
        return 1;
    glide::DirectoryLock lock;
    if (!lock.Acquire(stage)) {
        std::puts("Completion is already running");
        return 1;
    }
    // Bind resumable staging to exactly these inputs, preventing accidental reuse.
    glide::Buffer inputManifest;
    if (!glide::ReadFileBytes(inputsRoot + L"\\installation.txt", 16 * 1024 * 1024, inputManifest))
        return 1;
    const auto marker = stage + L"\\installation.txt";
    if (GetFileAttributesW(marker.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (!glide::WriteFileAtomic(marker, inputManifest.View(), false))
            return 1;
    } else {
        glide::Buffer previous;
        if (!Regular(marker) || !glide::ReadFileBytes(marker, 16 * 1024 * 1024, previous) ||
            previous.Size() != inputManifest.Size() ||
            std::memcmp(previous.Data(), inputManifest.Data(), previous.Size())) {
            std::puts("Pending installation belongs to different import inputs; preserved");
            return 1;
        }
    }
    if (!Copy(executable, stage + L"\\" + manifest.executable))
        return 1;
    for (const auto& resource : manifest.resources)
        if (!Copy(game + L"\\" + resource, stage + L"\\" + resource))
            return 1;
    const auto sourceDat = game + L"\\" + manifest.archive, targetDat = stage + L"\\" + manifest.archive;
    if (GetFileAttributesW(targetDat.c_str()) == INVALID_FILE_ATTRIBUTES) {
        std::printf("Build %u: copying archive and adding %zu local data files\n", build->build, inputs.size());
        std::fflush(stdout);
        if (!glide::InstallationData::Create(sourceDat, targetDat, build->build, inputs, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
    }
    {
        glide::InstallationData data;
        glide::DatArchive native;
        if (!native.Open(targetDat) || !data.Open(targetDat, build->build, error) || data.Count() != inputs.size())
            return 1;
        for (const auto& input : inputs) {
            glide::Buffer expected, actual;
            if (!data.Read(input.name, actual, error) ||
                !glide::ReadFileBytes(input.source, 512 * 1024 * 1024, expected) || expected.Size() != actual.Size() ||
                std::memcmp(expected.Data(), actual.Data(), actual.Size()))
                return 1;
        }
    }
    lock.Release();
    // The DAT and all auxiliary components become visible as one installation.
    if (!MoveFileExW(stage.c_str(), output.c_str(), MOVEFILE_WRITE_THROUGH)) {
        std::puts("Cannot publish installation; pending directory is preserved for retry");
        return 1;
    }
    std::printf("Build %u: completed installation created; original installation preserved\n", build->build);
    return 0;
}
