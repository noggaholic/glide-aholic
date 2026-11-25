#pragma once
#include "../../../client/src/profile.h"
#include "glide/installed_assets.h"
namespace glide {
struct JuneSymbol {
    const char* name;
    SymbolReference symbol;
};
struct JuneStartupInputs {
    uint32_t build = 0;
    InstalledAssets* assets = nullptr;
    void* documents = nullptr;
    bool (*load)(void*, const std::wstring&, std::string&) = nullptr;
    bool (*prepare)(void*, ByteView, std::string&) = nullptr;
    ByteView (*document)(void*, const wchar_t*) = nullptr;
};
template <class Content>
JuneStartupInputs MakeJuneStartupInputs(uint32_t build, InstalledAssets& assets, Content& content) {
    return {build,
            &assets,
            &content,
            [](void* value, const std::wstring& path, std::string& error) {
                return static_cast<Content*>(value)->Load(path, error);
            },
            [](void* value, ByteView seed, std::string& error) {
                return static_cast<Content*>(value)->Prepare(seed, error);
            },
            [](void* value, const wchar_t* name) { return static_cast<Content*>(value)->Document(name); }};
}
const std::vector<JuneSymbol>& JuneStartupSymbols();
const std::vector<HookSpec>& JuneStartupHooks();
std::vector<HookSpec> MakeJuneStartupHooks(const std::vector<JuneSymbol>&);
bool PrepareJuneStartupInputs(ClientRuntime&, const PeImage&, void* module, std::string& error,
                              const std::vector<JuneSymbol>&, const JuneStartupInputs&);
bool PrepareJuneStartup(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
