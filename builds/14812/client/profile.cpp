#include "startup.h"
#include "exploration.h"
#include "../../../client/src/runtime.h"
#include "../build.h"
namespace glide {
namespace {
class JulyClient final : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14812;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const auto hooks = [] {
            auto result = JulyStartupHooks();
            const auto& controls = JulyExplorationHooks();
            result.insert(result.end(), controls.begin(), controls.end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareJulyStartup(runtime, image, module, error) &&
               PrepareJulyExploration(image, module, runtime.log, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        error.clear();
        return true;
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueJulyCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        ToggleJulyFog();
    }
};
} // namespace
const ClientProfile& July14812Client() {
    static JulyClient profile;
    return profile;
}
} // namespace glide
