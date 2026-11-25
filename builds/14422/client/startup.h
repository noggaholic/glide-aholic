#pragma once
#include "../../14505/client/startup.h"
namespace glide {
namespace june14422 {
const std::vector<JuneSymbol>& StartupSymbols();
const std::vector<HookSpec>& StartupHooks();
bool PrepareStartup(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace june14422
} // namespace glide
