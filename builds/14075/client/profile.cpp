#include "client_startup.h"
#include "../build.h"
#include "../../../client/src/runtime.h"

namespace glide {
namespace april14075 {
namespace {
class AprilClient final : public ClientProfile {
  public:
    const BuildInfo& Info() const override {
        return kBuild14075;
    }
    CapabilitySet Capabilities() const override {
        return CapabilitySet::Of({Capability::FreeCamera, Capability::Fog});
    }
    const std::vector<HookSpec>& CodeHooks() const override {
        static const std::vector<HookSpec> hooks = [] {
            auto result = NativeResourceHooks();
            const auto& startup = ClientStartupHooks();
            result.insert(result.end(), startup.begin(), startup.end());
            return result;
        }();
        return hooks;
    }
    const wchar_t* LauncherUrlToPatch() const override {
        return nullptr;
    }
    bool Prepare(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) const override {
        return PrepareClientStartup(runtime, image, module, error);
    }
    bool AfterHooks(ClientRuntime&, std::string& error) const override {
        return FinishClientStartup(error);
    }
    void ToggleFreeCamera(ClientRuntime&) const override {
        QueueCamera();
    }
    void ToggleFog(ClientRuntime&) const override {
        april14075::ToggleFog();
    }
};
} // namespace
const ClientProfile& ClientProfile14075() {
    static AprilClient profile;
    return profile;
}
} // namespace april14075
} // namespace glide
