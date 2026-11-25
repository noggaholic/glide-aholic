#include "control_symbols.h"
namespace glide {
namespace china167409 {
namespace {} // namespace
const std::vector<StartupSymbol>& ControlSymbols() {
    static const std::vector<StartupSymbol> result = {{"ChinaMovementTransition", {"ChinaMovementTransition"}},
                                                      {"ChinaMovementPolicy", {"ChinaMovementPolicy"}},
                                                      {"ChinaGliderPermission", {"ChinaGliderPermission"}},
                                                      {"ChinaCollisionCreate", {"ChinaCollisionCreate"}},
                                                      {"ChinaCollisionMove", {"ChinaCollisionMove"}},
                                                      {"ChinaCubeMapQuality", {"ChinaCubeMapQuality"}},
                                                      {"ChinaPartyRoot", {"ChinaPartyRoot"}},
                                                      {"ChinaGameModeType8", {"ChinaGameModeType8"}},
                                                      {"ChinaUiServiceRoot", {"ChinaUiServiceRoot"}},
                                                      {"ChinaGameplayDispatch", {"ChinaGameplayDispatch"}},
                                                      {"ChinaDebugCamera", {"ChinaDebugCamera"}},
                                                      {"ChinaFogSubmit", {"ChinaFogSubmit"}},
                                                      {"ChinaFogDisable", {"ChinaFogDisable"}}};
    return result;
}
} // namespace china167409
} // namespace glide
