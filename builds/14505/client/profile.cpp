#include "startup.h"
#include "exploration.h"
#include "../../../client/src/runtime.h"
#include "../build.h"

namespace glide {
namespace {
class JuneClient : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14505;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            auto result = JuneStartupHooks();
            const auto& exploration = JuneExplorationHooks();
            result.insert(result.end(), exploration.begin(), exploration.end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareJuneStartup(runtime, image, module, error) &&
               PrepareJuneExploration(image, module, runtime.log, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        error.clear();
        return true;
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueJuneCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        ToggleJuneFog();
    }
};
} // namespace
const ClientProfile& June14505Client() {
    static JuneClient profile;
    return profile;
}
} // namespace glide
