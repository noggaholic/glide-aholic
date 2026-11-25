#pragma once
#include "glide/symbols.h"

namespace glide {
enum class LoginSurface { NativeScreen, Coherent, Cef, Browser };
struct LaunchInfo {
    const wchar_t* extraArguments;
    uint32_t readyTimeoutMs;
    LoginSurface login;
    const wchar_t* runtimeManifest;
};
struct BuildInfo {
    uint32_t build;
    Bitness bitness;
    const char* name;
    SymbolReference buildGetter;
    LaunchInfo launch;
};
class BuildTable {
  public:
    static size_t Count();
    static const BuildInfo* At(size_t index);
    static const BuildInfo* Find(uint32_t build);
};
enum class BuildDetection { Found, Unsupported, Ambiguous, OverrideMismatch };
BuildDetection DetectBuild(const PeImage& image, uint32_t overrideBuild, const BuildInfo*& out);
} // namespace glide
