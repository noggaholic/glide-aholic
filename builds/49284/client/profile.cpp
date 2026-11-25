#include "startup.h"
#include "archive.h"
#include "transport.h"
#include "browser.h"
#include "map_policy.h"
#include "assets.h"
#include "controls.h"
#include "exploration.h"
#include "../build.h"
#include "../../../client/src/runtime.h"
#include "../../../client/src/instance_hooks.h"

namespace glide {
namespace {
class HotClient : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild49284;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog, Capability::Glider});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            std::vector<HookSpec> result;
            for (const auto* group :
                 {&HotStartupHooks(), &HotArchiveHooks(), &HotTransportHooks(), &HotBrowserHooks(),
                  &HotMapPolicyHooks(), &HotAssetHooks(), &HotControlHooks(), &HotExplorationHooks()})
                result.insert(result.end(), group->begin(), group->end());
            return result;
        }();
        return hooks;
    }
    // PrepareHotBrowser installs the verified x86 URL patch and local login page.
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareHotStartup(runtime, image, module, error) && PrepareHotArchive(runtime, image, module, error) &&
               PrepareHotTransport(runtime, image, module, error) && PrepareHotBrowser(runtime, image, module, error) &&
               PrepareHotMapPolicy(runtime, image, module, error) && PrepareHotAssets(runtime, image, module, error) &&
               PrepareHotControls(runtime, image, module, error) &&
               PrepareHotExploration(image, module, runtime.log, error) &&
               InstallInstanceHooks(runtime.hooks, runtime.config.build, runtime.config.slot, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        error.clear();
        return true;
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueHotCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        ToggleHotFog();
    }
};
} // namespace
const ClientProfile& Hot49284Client() {
    static HotClient profile;
    return profile;
}
} // namespace glide
