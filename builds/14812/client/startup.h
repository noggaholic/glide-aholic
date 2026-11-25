#pragma once
#include "../../../client/src/profile.h"
namespace glide {
struct JulySymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<JulySymbol>& JulyStartupSymbols();
const std::vector<HookSpec>& JulyStartupHooks();
bool PrepareJulyStartup(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
