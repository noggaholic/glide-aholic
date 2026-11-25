#include "exploration.h"
namespace glide {
namespace june14422 {
const std::vector<JuneSymbol>& ExplorationSymbols() {
    static const std::vector<JuneSymbol> symbols = [] {
        auto result = JuneExplorationSymbols();
        result[2] = {"client_14422_debug_camera_toggle", {"client_14422_debug_camera_toggle"}};
        result[3] = {"client_14422_environment_fog_submit", {"client_14422_environment_fog_submit"}};
        result[4] = {"client_14422_environment_fog_disable", {"client_14422_environment_fog_disable"}};
        return result;
    }();
    return symbols;
}
const std::vector<HookSpec>& ExplorationHooks() {
    static const std::vector<HookSpec> hooks = MakeJuneExplorationHooks(ExplorationSymbols());
    return hooks;
}
bool PrepareExploration(const PeImage& image, void* module, Log& log, std::string& error) {
    return PrepareJuneExplorationFromSymbols(image, module, log, error, 14422, ExplorationSymbols());
}
void QueueCamera() {
    QueueJuneCamera();
}
void ToggleFog() {
    ToggleJuneFog();
}
} // namespace june14422
} // namespace glide
