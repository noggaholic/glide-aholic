#pragma once
#include "startup.h"
#include "glide/log.h"
namespace glide {
const std::vector<JulySymbol>& JulyExplorationSymbols();
const std::vector<HookSpec>& JulyExplorationHooks();
bool PrepareJulyExploration(const PeImage&, void* module, Log&, std::string& error);
void QueueJulyCamera();
void ToggleJulyFog();
} // namespace glide
