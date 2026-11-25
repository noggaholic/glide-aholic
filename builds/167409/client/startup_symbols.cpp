#include "startup_symbols.h"
#include <cstring>
namespace glide {
namespace china167409 {
namespace {} // namespace
const std::vector<StartupSymbol>& StartupSymbols() {
    static const std::vector<StartupSymbol> symbols = {{"ChinaArrayReserve", {"ChinaArrayReserve"}},
                                                       {"ChinaTlsServerHello", {"ChinaTlsServerHello"}},
                                                       {"ChinaRc4Ksa", {"ChinaRc4Ksa"}},
                                                       {"ChinaLocalArchive", {"ChinaLocalArchive"}},
                                                       {"ChinaMainArchive", {"ChinaMainArchive"}},
                                                       {"ChinaUserDataPath", {"ChinaUserDataPath"}},
                                                       {"ChinaCefExtract", {"ChinaCefExtract"}},
                                                       {"ChinaCefContextAsset", {"ChinaCefContextAsset"}},
                                                       {"ChinaUtf8ToWide", {"ChinaUtf8ToWide"}},
                                                       {"ChinaPatchState", {"ChinaPatchState"}},
                                                       {"ChinaArchiveAnalysis", {"ChinaArchiveAnalysis"}}};
    return symbols;
}
SymbolReference LauncherUrlSignature() {
    return {"launcher.url"};
}
} // namespace china167409
} // namespace glide
