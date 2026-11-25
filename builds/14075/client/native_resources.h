#pragma once
#include "assets.h"
#include "content.h"
#include "startup_symbols.h"
#include "../../../client/src/profile.h"

namespace glide {
namespace april14075 {
// Process-lifetime resources. The optional cache callback is used by the
// original-code comparison harness; production uses the private file writer.
struct NativeResourceInputs {
    uint32_t build = 0;
    Assets* assets = nullptr;
    ArchiveMembership membership;
    void* documents = nullptr;
    ByteView (*document)(void*, const wchar_t*) = nullptr;
    void** mainArchive = nullptr;
    std::wstring localPath;
    std::wstring emptyArchiveTemplate;
    void* cacheContext = nullptr;
    bool (*cache)(void*, const wchar_t*) = nullptr;
};
NativeResourceInputs MakeNativeResourceInputs(Assets&, Content&, void**, const std::wstring&);
bool ConfigureNativeResources(const NativeResourceInputs&, std::string& error);
bool NativeArchiveReady();
std::string NativeArchiveDiagnostic();
const std::vector<StartupSymbol>& NativeResourceSymbols();
const std::vector<HookSpec>& NativeResourceHooks();
bool PrepareNativeResources(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace april14075
} // namespace glide
