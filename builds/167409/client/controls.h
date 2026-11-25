#pragma once
#include "startup_symbols.h"
#include "../../../client/src/hooks.h"
#include "glide/log.h"
namespace glide {
namespace china167409 {
constexpr unsigned kCameraFlagsOffset = 0x3b0;
const std::vector<HookSpec>& ControlHooks();
bool PrepareControls(const PeImage& image, void* module, Log& log, std::string& error);
void QueueCamera();
void ToggleFog();
void SpacePressed();
bool StartHotkeys(std::string& error);
} // namespace china167409
} // namespace glide
