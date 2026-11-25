#pragma once
#include "startup.h"
namespace glide {
struct HotBrowserFile {
    const wchar_t* name;
    uint32_t baseId;
    size_t size;
    uint8_t hash[32];
};
const std::vector<HotBrowserFile>& HotBrowserFiles();
bool LoadHotBrowser(const std::wstring& directory, std::wstring& script, std::string& error);
// Exact name and native file reference must both match the retained middleware.
const HotBrowserFile* FindHotBrowserFile(const wchar_t* name, const unsigned short* reference);
const std::vector<HotSymbol>& HotBrowserSymbols();
const std::vector<HookSpec>& HotBrowserHooks();
SymbolReference HotLauncherUrlSignature();
bool PrepareHotBrowser(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
