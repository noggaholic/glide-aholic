#include "asset_symbols.h"
namespace glide {
namespace china167409 {
namespace {

// Main provider used by the retained ChinaMainManifest lookup.
// Related strings: !stream, MultiArchiveLocal.cpp; MultiArchive source component.

} // namespace
const std::vector<StartupSymbol>& AssetSymbols() {
    static const std::vector<StartupSymbol> symbols = {
        {"ChinaExeFileOpen", {"ChinaExeFileOpen"}},       {"ChinaAssetInfo", {"ChinaAssetInfo"}},
        {"ChinaLocalManifest", {"ChinaLocalManifest"}},   {"ChinaMainManifest", {"ChinaMainManifest"}},
        {"ChinaTextureVariant", {"ChinaTextureVariant"}}, {"ChinaArchiveRoute", {"ChinaArchiveRoute"}},
        {"ChinaMapPatchTask", {"ChinaMapPatchTask"}},     {"ChinaManifestPriority", {"ChinaManifestPriority"}},
        {"ChinaMainProvider", {"ChinaMainProvider"}}};
    return symbols;
}
} // namespace china167409
} // namespace glide
