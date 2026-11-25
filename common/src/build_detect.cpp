#include "glide/builds.h"

namespace glide {
BuildDetection DetectBuild(const PeImage& image, uint32_t overrideBuild, const BuildInfo*& out) {
    out = nullptr;
    const auto* profile = FindExecutableProfile(image);
    const BuildInfo* found = profile ? BuildTable::Find(profile->build) : nullptr;
    if (!found || found->bitness != image.Machine())
        return BuildDetection::Unsupported;
    if (overrideBuild && overrideBuild != found->build)
        return BuildDetection::OverrideMismatch;
    out = found;
    return BuildDetection::Found;
}
} // namespace glide
