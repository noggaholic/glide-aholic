#include "archive.h"
#include "private_cache.h"
#include "../../../client/src/runtime.h"
#include <Windows.h>
#include <intrin.h>
#include <cstring>

namespace glide {
namespace {
Glide49284PrivateCache cache = {};
Glide49284DirectoryFn originalDirectory = nullptr;
Glide49284CacheOpenFn originalOpen = nullptr;
std::wstring directory, path;
bool prepared = false;
void __fastcall Directory(unsigned selector, wchar_t* output, unsigned capacity) {
    if (!glide_49284_cache_directory(&cache, originalDirectory, reinterpret_cast<uintptr_t>(_ReturnAddress()), selector,
                                     output, capacity)) {
        if (output && capacity)
            output[0] = 0;
        SetLastError(ERROR_INSUFFICIENT_BUFFER);
    }
}
void* __fastcall Open(unsigned flags, const wchar_t*) {
    return glide_49284_open_private_cache(&cache, originalOpen, flags);
}
} // namespace
const std::vector<HotSymbol>& HotArchiveSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {SYMBOL(client_49284_cache_directory),
                                                   SYMBOL(client_49284_cache_open), SYMBOL(client_49284_cache_paths)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotArchiveHooks() {
    static const std::vector<HookSpec> hooks = {
        {HotArchiveSymbols()[0].name, HotArchiveSymbols()[0].symbol, reinterpret_cast<void*>(Directory),
         reinterpret_cast<void**>(&originalDirectory), true},
        {HotArchiveSymbols()[1].name, HotArchiveSymbols()[1].symbol, reinterpret_cast<void*>(Open),
         reinterpret_cast<void**>(&originalOpen), true}};
    return hooks;
}
bool ResolveHotArchive(const PeImage& image, std::array<uint32_t, 3>& offsets, std::string& error) {
    error = "HoT archive requires an x86 image";
    if (image.Machine() != Bitness::X86)
        return false;
    std::array<uint32_t, 3> candidate;
    for (size_t i = 0; i < candidate.size(); ++i) {
        const auto& symbol = HotArchiveSymbols()[i];
        const auto found = ResolveSymbol(image, symbol.symbol);
        if (found.count != 1) {
            error = std::string("HoT archive RVA is not verified: ") + symbol.name;
            return false;
        }
        candidate[i] = found.rva;
    }
    error = "HoT cache folder calls do not target the verified native function";
    bool verified = false;
    for (const auto& section : image.Code()) {
        if (candidate[2] < section.rva)
            continue;
        const size_t at = candidate[2] - section.rva;
        if (at > section.bytes.size || section.bytes.size - at < 61)
            continue;
        const uint8_t* bytes = section.bytes.data + at;
        int32_t first, second;
        std::memcpy(&first, bytes + 12, 4);
        std::memcpy(&second, bytes + 57, 4);
        if (bytes[11] != 0xe8 || bytes[56] != 0xe8 || int64_t(candidate[2]) + 16 + first != candidate[0] ||
            int64_t(candidate[2]) + 61 + second != candidate[0])
            return false;
        verified = true;
        break;
    }
    if (!verified)
        return false;
    offsets = candidate;
    error.clear();
    return true;
}
bool PrepareHotArchive(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT private cache requires its launcher-owned x86 working copy";
    if (prepared || !module || sizeof(void*) != 4 || runtime.config.build != 49284 || !runtime.config.slot)
        return false;
    wchar_t executable[32768];
    const DWORD count = GetModuleFileNameW(static_cast<HMODULE>(module), executable, 32768);
    const std::wstring expected = runtime.config.runtimeDirectory + L"\\Gw2.exe";
    const std::wstring suffix = L"\\49284\\client-" + std::to_wstring(runtime.config.slot);
    const auto& runtimeDirectory = runtime.config.runtimeDirectory;
    if (!count || count >= 32768 || _wcsicmp(executable, expected.c_str()) || runtimeDirectory.size() < suffix.size() ||
        _wcsicmp(runtimeDirectory.c_str() + runtimeDirectory.size() - suffix.size(), suffix.c_str()))
        return false;
    std::array<uint32_t, 3> offsets;
    if (!ResolveHotArchive(image, offsets, error))
        return false;
    directory = runtimeDirectory;
    path = directory + L"\\Local.dat";
    cache.directory = directory.c_str();
    cache.path = path.c_str();
    cache.first_directory_return = reinterpret_cast<uintptr_t>(module) + offsets[2] + 16;
    cache.second_directory_return = reinterpret_cast<uintptr_t>(module) + offsets[2] + 61;
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
