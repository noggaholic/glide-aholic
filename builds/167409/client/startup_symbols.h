#pragma once
#include "glide/symbols.h"
#include <vector>
namespace glide {
namespace china167409 {
struct StartupSymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<StartupSymbol>& StartupSymbols();
SymbolReference LauncherUrlSignature();
} // namespace china167409
} // namespace glide
