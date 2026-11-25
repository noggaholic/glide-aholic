#pragma once
#include "../../../client/src/runtime.h"
namespace glide {
namespace china167409 {
// This limited fallback does not recover the missing original map imagery.
bool CartographyUnavailable(const float* layout);
void CartographyOrigin(const float* layout, float* output);
const std::vector<HookSpec>& CartographyHooks();
bool PrepareCartography(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error);
} // namespace china167409
} // namespace glide
