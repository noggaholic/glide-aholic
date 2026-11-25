#pragma once
#include "startup.h"
#include "glide/log.h"

namespace glide {
const std::vector<StartupSymbol>& Build204489ExplorationSymbols();
const std::vector<HookSpec>& Build204489ExplorationHooks();
// Called by foreground physical input monitoring on a new Space press.
void Build204489SpacePressed();
bool PrepareBuild204489Exploration(const PeImage& image, void* module, Log& log, std::string& error);
void QueueBuild204489Camera();
void ToggleBuild204489Fog();
bool StartBuild204489Hotkeys(std::string& error);
} // namespace glide
