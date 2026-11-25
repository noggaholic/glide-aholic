#include "native_resources.h"
#include <cstring>

namespace glide {
namespace april14075 {
const std::vector<StartupSymbol>& NativeResourceSymbols() {
    static const std::vector<StartupSymbol> symbols = [] {
        const char* names[] = {"beta_open_local_dat",
                               "beta_archive_write_file_raw",
                               "beta_msgconn_rc4",
                               "beta_content_key_derive",
                               "beta_content_open_packs",
                               "beta_content_manifest_reader",
                               "beta_content_record_lookup",
                               "beta_gc_game_join_result",
                               "beta_archive_file_availability",
                               "beta_archive_reference_resolve",
                               "beta_archive_reference_decode",
                               "beta_local_manifest_initialize",
                               "beta_prepare_archive"};
        std::vector<StartupSymbol> result;
        for (const auto* name : names)
            for (const auto& symbol : StartupSymbols())
                if (!std::strcmp(name, symbol.name))
                    result.push_back(symbol);
        result.push_back({"beta_archive_publisher", {"beta_archive_publisher"}});
        return result;
    }();
    return symbols;
}
} // namespace april14075
} // namespace glide
