#include "prepare.h"
#include "prepare_options.h"
#include "glide/builds.h"
#include "glide/files.h"
#include "glide/installation_data.h"
#include "glide/runtime_manifest.h"
#include <Windows.h>
#include <cstdio>

namespace {
bool Absolute(const std::wstring& value, std::wstring& out) {
    wchar_t path[32768];
    const DWORD count = GetFullPathNameW(value.c_str(), 32768, path, nullptr);
    if (!count || count >= 32768)
        return false;
    out.assign(path, count);
    return true;
}
bool Under(const std::wstring& path, const std::wstring& root) {
    return path.size() >= root.size() && !_wcsnicmp(path.c_str(), root.c_str(), root.size()) &&
           (path.size() == root.size() || path[root.size()] == L'\\');
}
bool VerifyRuntimeInputs(unsigned build, const std::wstring& game, const std::wstring& prepared, std::string& error) {
    glide::Buffer bytes;
    glide::RuntimeManifest manifest;
    error = "Cannot read runtime resource manifest";
    if (!glide::ReadFileBytes(L"builds/" + std::to_wstring(build) + L"/runtime-manifest.txt", 1024 * 1024, bytes) ||
        !glide::RuntimeManifest::Parse(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()), manifest,
                                       error) ||
        manifest.build != build)
        return false;
    for (const auto& resource : manifest.resources) {
        const auto path = game + L"/" + resource;
        const auto attributes = GetFileAttributesW(path.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES ||
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))) {
            std::string name;
            glide::WideToUtf8(resource, name);
            error = "Missing or invalid original runtime resource: " + name;
            return false;
        }
    }
    for (const auto& package : manifest.packages)
        if (!package.Verify(prepared + L"/content/" + package.source, error))
            return false;
    error.clear();
    return true;
}
} // namespace
int PrepareMain(int argc, wchar_t** argv) {
    if (argc < 2) {
        std::puts("Usage: glide-catalog prepare <game-executable> [--build N] [--out data-directory]");
        return 2;
    }
    PrepareOptions options;
    const int parsed = ParsePrepareOptions(argc, argv, options);
    if (parsed)
        return parsed;
    std::wstring executable, destination;
    const glide::BuildInfo* build = nullptr;
    glide::PeImage image;
    if (!Absolute(argv[1], executable) || !glide::PeImage::FromFile(executable.c_str(), image) ||
        glide::DetectBuild(image, options.requested, build) != glide::BuildDetection::Found || !build) {
        std::puts("Unsupported or mismatched game executable");
        return 1;
    }
    const auto game = executable.substr(0, executable.find_last_of(L"/\\"));
    if (!Absolute(options.output, destination))
        return 2;
    destination += L"/" + std::to_wstring(build->build);
    std::wstring canonical;
    if (!Absolute(destination, canonical) || Under(canonical, game)) {
        std::puts("Output must be outside the game installation");
        return 1;
    }
    glide::InstallationData installed;
    std::string error;
    if (!installed.Open(game + L"/Gw2.dat", build->build, error)) {
        std::puts("This installation has no verified local data; complete it before preparation");
        return 1;
    }
    if (!installed.Extract(canonical, error) || !VerifyRuntimeInputs(build->build, game, canonical, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("Build %u: %zu files prepared from the completed installation\n", build->build, installed.Count());
    return 0;
}
