#pragma once
#include "glide/symbols.h"
#include <vector>
namespace glide {
namespace april14075 {
struct StartupSymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<StartupSymbol>& StartupSymbols();
} // namespace april14075
} // namespace glide
