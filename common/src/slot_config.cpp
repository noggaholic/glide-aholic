#include "glide/slot_config.h"
#include "glide/builds.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cwchar>

namespace glide {
bool EnvironmentLess::operator()(const std::wstring& a, const std::wstring& b) const {
    return _wcsicmp(a.c_str(), b.c_str()) < 0;
}
bool ReadEnvironment(Environment& out) {
    wchar_t* block = GetEnvironmentStringsW();
    if (!block)
        return false;
    Environment result;
    for (const wchar_t* at = block; *at; at += std::wcslen(at) + 1) {
        const std::wstring entry(at);
        const size_t equals = entry.find(L'=', entry[0] == L'=' ? 1 : 0);
        if (equals == std::wstring::npos) {
            FreeEnvironmentStringsW(block);
            return false;
        }
        result[entry.substr(0, equals)] = entry.substr(equals + 1);
    }
    FreeEnvironmentStringsW(block);
    out = std::move(result);
    return true;
}
bool EnvironmentBlock(const Environment& values, std::vector<wchar_t>& out) {
    std::vector<wchar_t> block;
    for (const auto& entry : values) {
        const auto& key = entry.first;
        if (key.empty() || key.find(L'\0') != std::wstring::npos || entry.second.find(L'\0') != std::wstring::npos)
            return false;
        // Preserve Windows' hidden entries, including drive directories and cmd's
        // =ExitCode. Their leading '=' is part of the name, not the separator.
        if (key == L"=" || key.find(L'=', key[0] == L'=' ? 1 : 0) != std::wstring::npos)
            return false;
        if (key.size() > 32767 || entry.second.size() > 32767 ||
            block.size() + key.size() + entry.second.size() + 3 > 32767)
            return false;
        block.insert(block.end(), key.begin(), key.end());
        block.push_back(L'=');
        block.insert(block.end(), entry.second.begin(), entry.second.end());
        block.push_back(0);
    }
    if (block.empty())
        block.push_back(0);
    block.push_back(0);
    out = std::move(block);
    return true;
}
namespace {
bool Number(const Environment& environment, const wchar_t* name, unsigned maximum, unsigned& out) {
    const auto at = environment.find(name);
    if (at == environment.end() || at->second.empty())
        return false;
    unsigned value = 0;
    for (wchar_t digit : at->second) {
        if (digit < L'0' || digit > L'9' || value > (maximum - static_cast<unsigned>(digit - L'0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(digit - L'0');
    }
    if (!value)
        return false;
    out = value;
    return true;
}
bool Path(const Environment& environment, const wchar_t* name, std::wstring& out) {
    const auto at = environment.find(name);
    if (at == environment.end())
        return false;
    const auto& value = at->second;
    if (value.size() < 3 || value[1] != L':' || (value[2] != L'\\' && value[2] != L'/') ||
        value.find(L'\0') != std::wstring::npos)
        return false;
    wchar_t full[32768];
    const DWORD count = GetFullPathNameW(value.c_str(), 32768, full, nullptr);
    if (!count || count >= 32768)
        return false;
    out = full;
    while (out.size() > 3 && out.back() == L'\\')
        out.pop_back();
    return true;
}
} // namespace
bool SlotConfig::Decode(const Environment& environment, SlotConfig& out, std::string& error) {
    error = "Missing or invalid client slot configuration";
    SlotConfig config;
    if (!Number(environment, L"GLIDE_BUILD", UINT32_MAX, config.build) || !BuildTable::Find(config.build) ||
        !Number(environment, L"GLIDE_SLOT", 999999, config.slot) ||
        !Number(environment, L"GLIDE_SERVER_PORT", 65535, config.serverPort) ||
        !Number(environment, L"GLIDE_WEB_PORT", 65535, config.webPort) || config.serverPort == config.webPort ||
        config.webPort == 6112)
        return false;
    const auto host = environment.find(L"GLIDE_SERVER_HOST");
    in_addr address = {};
    if (host == environment.end() || host->second.find(L'\0') != std::wstring::npos ||
        InetPtonW(AF_INET, host->second.c_str(), &address) != 1)
        return false;
    const ULONG numeric = ntohl(address.s_addr);
    if (!numeric || (numeric >> 24) == 0 || (numeric >> 28) >= 14 ||
        (numeric == INADDR_LOOPBACK && config.serverPort == 80))
        return false;
    config.serverHost = host->second;
    if (!Path(environment, L"GLIDE_RUNTIME_DIR", config.runtimeDirectory) ||
        !Path(environment, L"GLIDE_DATA_DIR", config.dataDirectory) ||
        !Path(environment, L"GLIDE_LOG", config.logPath) || !Path(environment, L"GLIDE_ARCHIVE", config.archivePath) ||
        !Path(environment, L"GLIDE_MANIFEST", config.manifestPath) ||
        !Path(environment, L"GLIDE_SOURCE_EXE", config.sourceExecutable))
        return false;
    const std::wstring prefix = config.runtimeDirectory + L"\\";
    if (config.logPath.size() <= prefix.size() || _wcsnicmp(config.logPath.c_str(), prefix.c_str(), prefix.size())) {
        error = "Client log must be inside its private runtime directory";
        return false;
    }
    out = std::move(config);
    error.clear();
    return true;
}
bool SlotConfig::MakeEnvironment(const Environment& inherited, std::vector<wchar_t>& out, std::string& error) const {
    Environment environment = inherited;
    environment[L"GLIDE_BUILD"] = std::to_wstring(build);
    environment[L"GLIDE_SLOT"] = std::to_wstring(slot);
    environment[L"GLIDE_SERVER_HOST"] = serverHost;
    environment[L"GLIDE_SERVER_PORT"] = std::to_wstring(serverPort);
    environment[L"GLIDE_WEB_PORT"] = std::to_wstring(webPort);
    environment[L"GLIDE_RUNTIME_DIR"] = runtimeDirectory;
    environment[L"GLIDE_DATA_DIR"] = dataDirectory;
    environment[L"GLIDE_LOG"] = logPath;
    environment[L"GLIDE_ARCHIVE"] = archivePath;
    environment[L"GLIDE_MANIFEST"] = manifestPath;
    environment[L"GLIDE_SOURCE_EXE"] = sourceExecutable;
    SlotConfig verified;
    if (!Decode(environment, verified, error))
        return false;
    if (!EnvironmentBlock(environment, out)) {
        error = "Invalid or oversized process environment";
        return false;
    }
    error.clear();
    return true;
}
} // namespace glide
