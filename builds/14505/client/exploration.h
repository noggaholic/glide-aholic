#pragma once
#include "startup.h"
#include "glide/log.h"

namespace glide {
const std::vector<JuneSymbol>& JuneExplorationSymbols();
const std::vector<HookSpec>& JuneExplorationHooks();
std::vector<HookSpec> MakeJuneExplorationHooks(const std::vector<JuneSymbol>&);
bool PrepareJuneExplorationFromSymbols(const PeImage&, void* module, Log&, std::string& error, uint32_t build,
                                       const std::vector<JuneSymbol>&);
bool PrepareJuneExploration(const PeImage& image, void* module, Log& log, std::string& error);
void QueueJuneCamera();
void ToggleJuneFog();
} // namespace glide
