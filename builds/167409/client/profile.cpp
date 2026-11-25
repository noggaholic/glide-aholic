#include "startup.h"
#include "asset_hooks.h"
#include "cartography.h"
#include "controls.h"
#include "../build.h"
#include "../../../client/src/profile.h"
namespace glide {
namespace china167409 {
namespace {
class ChinaClient final : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild167409;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog, Capability::Glider});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            std::vector<HookSpec> result;
            for (const auto* group : {&StartupHooks(), &AssetHooks(), &CartographyHooks(), &ControlHooks()})
                result.insert(result.end(), group->begin(), group->end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareStartup(runtime, image, module, error) && PrepareAssets(runtime, image, module, error) &&
               PrepareCartography(runtime, image, module, error) &&
               PrepareControls(image, module, runtime.log, error) &&
               PrepareLauncherUrl(runtime.launcherUrl, image, module, runtime.config.webPort, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        return StartHotkeys(error);
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        china167409::ToggleFog();
    }
};
} // namespace
const ClientProfile& ClientProfile167409() {
    static ChinaClient profile;
    return profile;
}
} // namespace china167409
} // namespace glide
