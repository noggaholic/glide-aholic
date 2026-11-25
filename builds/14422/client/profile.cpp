#include "startup.h"
#include "exploration.h"
#include "../build.h"
#include "../../../client/src/runtime.h"
namespace glide {
namespace {
class June7Client final : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14422;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            auto result = june14422::StartupHooks();
            const auto& controls = june14422::ExplorationHooks();
            result.insert(result.end(), controls.begin(), controls.end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return june14422::PrepareStartup(runtime, image, module, error) &&
               june14422::PrepareExploration(image, module, runtime.log, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        error.clear();
        return true;
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        june14422::QueueCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        june14422::ToggleFog();
    }
};
} // namespace
const ClientProfile& June14422Client() {
    static June7Client profile;
    return profile;
}
} // namespace glide
