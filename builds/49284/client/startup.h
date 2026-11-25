#pragma once
#include "../../../client/src/profile.h"
namespace glide {
struct HotSymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<HotSymbol>& HotStartupSymbols();
const std::vector<HookSpec>& HotStartupHooks();
bool PrepareHotStartup(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
