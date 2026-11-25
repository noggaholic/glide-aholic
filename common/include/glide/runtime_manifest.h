#pragma once
#include <string>
#include <vector>

namespace glide {
bool RuntimeRelativePath(const std::wstring& path);
struct RuntimePackage {
    std::wstring source, destination;
    std::string sha256;
    bool Verify(const std::wstring& sourcePath, std::string& error) const;
};
struct RuntimeManifest {
    unsigned build = 0;
    std::vector<RuntimePackage> packages;
    std::wstring executable, archive;
    std::vector<std::wstring> resources, children;
    static bool Parse(const std::string& text, RuntimeManifest& out, std::string& error);
};
} // namespace glide
