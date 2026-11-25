#pragma once
#include "startup.h"
#include "glide/log.h"

namespace glide {
const std::vector<HotSymbol>& HotExplorationSymbols();
const std::vector<HookSpec>& HotExplorationHooks();
bool PrepareHotExploration(const PeImage& image, void* module, Log& log, std::string& error);
void QueueHotCamera();
void ToggleHotFog();
} // namespace glide
