#pragma once
#include "../../../client/src/hooks.h"
#include "glide/slot_config.h"
#include "../../../client/src/string_patch.h"

namespace glide {
struct StartupSymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<StartupSymbol>& Build204489StartupSymbols();
const std::vector<HookSpec>& Build204489StartupHooks();
// Resolves helpers, paths and export hooks. Code hooks are installed by the shared runtime.
bool PrepareBuild204489Startup(Hooks& hooks, const PeImage& image, void* module, const SlotConfig& config,
                               std::string& error);
// Native RC4 state starts with two 32-bit indices, followed by the 256-byte permutation.
void InitializeBuild204489Cipher(void* state);
SymbolReference Build204489LauncherUrlSignature();
bool PrepareBuild204489LauncherUrl(RelativeStringPatch& patch, const PeImage& image, void* module, unsigned webPort,
                                   std::string& error);
} // namespace glide
