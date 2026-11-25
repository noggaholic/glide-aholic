#include "../../../client/src/profile.h"
#include "../../../client/src/runtime.h"
#include "../build.h"
#include "startup.h"
#include "exploration.h"

namespace glide {
namespace {
class Client204489 : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild204489;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog, Capability::Glider});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            auto result = Build204489StartupHooks();
            const auto& exploration = Build204489ExplorationHooks();
            result.insert(result.end(), exploration.begin(), exploration.end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return L"coui://web/launcher/arenanet/index.html";
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareBuild204489Startup(runtime.hooks, image, module, runtime.config, error) &&
               PrepareBuild204489LauncherUrl(runtime.launcherUrl, image, module, runtime.config.webPort, error) &&
               PrepareBuild204489Exploration(image, module, runtime.log, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        return StartBuild204489Hotkeys(error);
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueBuild204489Camera();
    }
    void ToggleFog(ClientRuntime&) const override {
        ToggleBuild204489Fog();
    }
};
} // namespace
const ClientProfile& Build204489Client() {
    static Client204489 profile;
    return profile;
}
} // namespace glide
