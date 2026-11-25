#include "client_startup.h"
#include <cstring>

namespace glide {
namespace april14075 {
const std::vector<StartupSymbol>& ClientStartupSymbols() {
    static const std::vector<StartupSymbol> symbols = [] {
        const std::vector<StartupSymbol> extra = {
            {"beta_tls_server_hello", {"beta_tls_server_hello"}},
            {"beta_byte_array_assign", {"beta_byte_array_assign"}},
            {"beta_exe_patcher_run", {"beta_exe_patcher_run"}},
            {"beta_remote_exe_phase", {"beta_remote_exe_phase"}},
            {"beta_offline_manifest_branch", {"beta_offline_manifest_branch"}},
            {"beta_wait_for_launcher_play", {"beta_wait_for_launcher_play"}},
            {"beta_launcher_singleton", {"beta_launcher_singleton"}},
            {"beta_gameplay_dispatch", {"beta_gameplay_dispatch"}},
            {"beta_debug_camera_toggle", {"beta_debug_camera_toggle"}},
            {"beta_environment_fog_submit", {"beta_environment_fog_submit"}},
            {"beta_environment_fog_disable", {"beta_environment_fog_disable"}},
        };
        const char* names[] = {
            "beta_tls_server_hello",    "beta_byte_array_assign",       "beta_exe_patcher_run",
            "beta_remote_exe_phase",    "beta_offline_manifest_branch", "beta_wait_for_launcher_play",
            "beta_launcher_singleton",  "beta_view_set_state",          "beta_gameplay_dispatch",
            "beta_debug_camera_toggle", "beta_environment_fog_submit",  "beta_environment_fog_disable",
            "beta_dx9_create_device"};
        std::vector<StartupSymbol> result;
        for (const auto* name : names) {
            for (const auto& symbol : extra)
                if (!std::strcmp(name, symbol.name))
                    result.push_back(symbol);
            for (const auto& symbol : StartupSymbols())
                if (!std::strcmp(name, symbol.name))
                    result.push_back(symbol);
        }
        return result;
    }();
    return symbols;
}
} // namespace april14075
} // namespace glide
