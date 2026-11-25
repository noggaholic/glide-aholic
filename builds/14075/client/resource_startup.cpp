#include "native_resources.h"
#include "../build.h"
#include "../../../client/src/runtime.h"
#include <Windows.h>
#include <cstring>

namespace glide {
namespace april14075 {
namespace {
Assets assets;
Content content;
} // namespace
bool PrepareNativeResources(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "April native resources require the original matching x86 executable";
    if (!module || sizeof(void*) != 4 || runtime.config.build != 14075 || image.Machine() != Bitness::X86 ||
        ResolveSymbol(image, kBuild14075.buildGetter).count != 1)
        return false;
    const auto& symbols = NativeResourceSymbols();
    if (symbols.size() != 14)
        return false;
    uint32_t publisher = 0;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("April resource RVA is not verified: ") + symbol.name;
            return false;
        }
        publisher = match.rva;
    }
    uint32_t slot = 0;
    std::memcpy(&slot, static_cast<uint8_t*>(module) + publisher + 12, 4);
    MEMORY_BASIC_INFORMATION region = {};
    error = "April archive slot is outside writable image data";
    if (!VirtualQuery(reinterpret_cast<void*>(uintptr_t(slot)), &region, sizeof(region)) ||
        region.AllocationBase != module || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
        return false;
    if (!content.Load(runtime.config.dataDirectory + L"/content", error) || !content.Prepare(error) ||
        !assets.Open(runtime.config.archivePath, error))
        return false;
    auto selected = MakeNativeResourceInputs(assets, content, reinterpret_cast<void**>(uintptr_t(slot)),
                                             runtime.config.runtimeDirectory + L"/Local.dat");
    selected.emptyArchiveTemplate = runtime.config.dataDirectory + L"/client/empty-cache.bin";
    return ConfigureNativeResources(selected, error);
}
} // namespace april14075
} // namespace glide
