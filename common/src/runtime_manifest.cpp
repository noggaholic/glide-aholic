#include "glide/runtime_manifest.h"
#include "glide/builds.h"
#include "glide/files.h"
#include "glide/kv.h"
#include "glide/crypto.h"
#include <set>
#include <cwctype>
#include <cwchar>

namespace glide {
bool RuntimeRelativePath(const std::wstring& path) {
    if (path.empty() || path.size() > 240)
        return false;
    size_t start = 0;
    for (size_t i = 0; i <= path.size(); ++i) {
        if (i < path.size() && path[i] != L'\\') {
            if (path[i] < 32 || std::wcschr(L"/:*?\"<>|", path[i]))
                return false;
            continue;
        }
        const std::wstring part = path.substr(start, i - start);
        if (part.empty() || part == L"." || part == L".." || part.back() == L'.' || part.back() == L' ')
            return false;
        std::wstring stem = part.substr(0, part.find(L'.'));
        for (auto& c : stem)
            c = static_cast<wchar_t>(std::towupper(c));
        if (!_wcsicmp(part.c_str(), L".slot.lock") || stem == L"CON" || stem == L"PRN" || stem == L"AUX" ||
            stem == L"NUL" ||
            (stem.size() == 4 && (stem.substr(0, 3) == L"COM" || stem.substr(0, 3) == L"LPT") && stem[3] >= L'0' &&
             stem[3] <= L'9') ||
            part == L".slot.lock")
            return false;
        start = i + 1;
    }
    return true;
}
} // namespace glide
namespace glide {
namespace {
std::wstring Key(std::wstring value) {
    for (auto& c : value)
        c = static_cast<wchar_t>(std::towupper(c));
    return value;
}
bool ResourcePath(const std::string& text, std::wstring& out) {
    return Utf8ToWide(text, out) && RuntimeRelativePath(out);
}
} // namespace
bool RuntimePackage::Verify(const std::wstring& sourcePath, std::string& error) const {
    error = "Packaged runtime resource is missing or its hash differs";
    Buffer bytes;
    uint8_t hash[32];
    if (!ReadFileBytes(sourcePath, 128 * 1024 * 1024, bytes) || !Sha256(bytes.View(), hash))
        return false;
    std::string actual;
    for (uint8_t byte : hash) {
        actual += "0123456789abcdef"[byte >> 4];
        actual += "0123456789abcdef"[byte & 15];
    }
    if (actual != sha256)
        return false;
    error.clear();
    return true;
}
bool RuntimeManifest::Parse(const std::string& text, RuntimeManifest& out, std::string& error) {
    error = "Invalid runtime resource manifest";
    KvDocument document;
    std::string parseError, value;
    if (!KvDocument::Parse(text, document, parseError) || !document.Sections().empty() ||
        !document.Get("", "format", value) || (value != "1" && value != "2"))
        return false;
    const bool packagedFormat = value == "2";
    RuntimeManifest manifest;
    if (!document.Get("", "build", value) || !ParseUnsigned(value, UINT32_MAX, manifest.build) ||
        !BuildTable::Find(manifest.build) || !document.Get("", "executable", value) ||
        !ResourcePath(value, manifest.executable) || !document.Get("", "archive", value) ||
        !ResourcePath(value, manifest.archive))
        return false;
    std::set<std::wstring> destinations{Key(manifest.executable), Key(manifest.archive)}, children;
    if (destinations.size() != 2 || manifest.executable.find(L'\\') != std::wstring::npos)
        return false;
    for (const auto& entry : document.Entries("")) {
        if (entry.first == "format" || entry.first == "build" || entry.first == "executable" ||
            entry.first == "archive")
            continue;
        if (entry.first == "package") {
            if (!packagedFormat)
                return false;
            const size_t first = entry.second.find('|'), last = entry.second.rfind('|');
            RuntimePackage package;
            if (first == std::string::npos || first == last ||
                !ResourcePath(entry.second.substr(0, first), package.source) ||
                !ResourcePath(entry.second.substr(first + 1, last - first - 1), package.destination))
                return false;
            package.sha256 = entry.second.substr(last + 1);
            if (package.sha256.size() != 64 ||
                package.sha256.find_first_not_of("0123456789abcdef") != std::string::npos ||
                !destinations.insert(Key(package.destination)).second)
                return false;
            manifest.packages.push_back(std::move(package));
            continue;
        }
        std::wstring path;
        if (!ResourcePath(entry.second, path))
            return false;
        if (entry.first == "resource") {
            if (!destinations.insert(Key(path)).second)
                return false;
            manifest.resources.push_back(std::move(path));
        } else if (entry.first == "child") {
            if (!children.insert(Key(path)).second)
                return false;
            manifest.children.push_back(std::move(path));
        } else
            return false;
    }
    for (const auto& child : children)
        if (child == Key(manifest.executable) || child == Key(manifest.archive) || !destinations.count(child))
            return false;
    for (const auto& path : destinations) {
        for (size_t slash = path.find(L'\\'); slash != std::wstring::npos; slash = path.find(L'\\', slash + 1))
            if (destinations.count(path.substr(0, slash)))
                return false;
    }
    out = std::move(manifest);
    error.clear();
    return true;
}
} // namespace glide
