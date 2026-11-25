#include "startup.h"
#include "assets.h"
#include "content.h"
namespace glide {
namespace june14422 {
namespace {
Assets assets;
Content content;
} // namespace
const std::vector<JuneSymbol>& StartupSymbols() {
    static const std::vector<JuneSymbol> symbols = [] {
        auto result = JuneStartupSymbols();
        result.back() = {"client_14422_capability", {"client_14422_capability"}};
        return result;
    }();
    return symbols;
}
const std::vector<HookSpec>& StartupHooks() {
    static const std::vector<HookSpec> hooks = MakeJuneStartupHooks(StartupSymbols());
    return hooks;
}
bool PrepareStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    return PrepareJuneStartupInputs(runtime, image, module, error, StartupSymbols(),
                                    MakeJuneStartupInputs(14422, assets, content));
}
} // namespace june14422
} // namespace glide
