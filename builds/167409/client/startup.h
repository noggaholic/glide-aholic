#pragma once
#include "../../../client/src/hooks.h"
#include "../../../client/src/runtime.h"
#include "startup_symbols.h"
namespace glide {
namespace china167409 {
const std::vector<HookSpec>& StartupHooks();
bool PrepareStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error);
bool PrepareLauncherUrl(RelativeStringPatch& patch, const PeImage& image, void* module, unsigned webPort,
                        std::string& error);
} // namespace china167409
} // namespace glide
