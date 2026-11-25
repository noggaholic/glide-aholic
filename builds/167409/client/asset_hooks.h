#pragma once
#include "../../../client/src/runtime.h"
namespace glide {
namespace china167409 {
const std::vector<HookSpec>& AssetHooks();
bool PrepareAssets(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error);
void SourceContentReady();
} // namespace china167409
} // namespace glide
