#pragma once
#include <map>
#include <string>
#include <vector>

namespace glide {
struct EnvironmentLess {
    bool operator()(const std::wstring& a, const std::wstring& b) const;
};
using Environment = std::map<std::wstring, std::wstring, EnvironmentLess>;
bool ReadEnvironment(Environment& out);
bool EnvironmentBlock(const Environment& values, std::vector<wchar_t>& out);
struct SlotConfig {
    unsigned build = 0, slot = 0, serverPort = 0, webPort = 0;
    std::wstring serverHost, runtimeDirectory, dataDirectory, logPath, archivePath, manifestPath, sourceExecutable;
    static bool Decode(const Environment& environment, SlotConfig& out, std::string& error);
    bool MakeEnvironment(const Environment& inherited, std::vector<wchar_t>& out, std::string& error) const;
};
} // namespace glide
