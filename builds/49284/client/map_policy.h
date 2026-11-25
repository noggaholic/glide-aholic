#pragma once
#include "startup.h"
#include "installed.h"
#include "../server/map_catalog.h"
namespace glide {
bool VerifyHotMapAssets(const hot49284::MapGeometry&, Glide49284QueryAssetFn query);
const std::vector<HotSymbol>& HotMapPolicySymbols();
const std::vector<HookSpec>& HotMapPolicyHooks();
bool PrepareHotMapPolicy(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
