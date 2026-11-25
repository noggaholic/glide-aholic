#pragma once
#include "../../14505/client/exploration.h"
namespace glide {
namespace june14422 {
const std::vector<JuneSymbol>& ExplorationSymbols();
const std::vector<HookSpec>& ExplorationHooks();
bool PrepareExploration(const PeImage&, void* module, Log&, std::string& error);
void QueueCamera();
void ToggleFog();
} // namespace june14422
} // namespace glide
