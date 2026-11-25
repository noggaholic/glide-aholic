#include "browser.h"
#include "../../../client/src/runtime.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <Windows.h>
#include <cstring>
#include <cwchar>

namespace glide {
namespace {
using Extract = int(__fastcall*)(void*, void*, const wchar_t*, const unsigned short*, const wchar_t*, unsigned,
                                 wchar_t*, unsigned);
using Assign = void(__fastcall*)(unsigned*, void*, const wchar_t*, unsigned, unsigned);
Extract originalExtract = nullptr;
Assign assign = nullptr;
using LoginSuccess = void(__fastcall*)(void*, void*);
LoginSuccess originalLoginSuccess = nullptr;
void* originalScript = nullptr;
std::wstring directory, script;
bool prepared = false;
Log* browserLog = nullptr;
void __fastcall ObserveLoginSuccess(void* self, void* unused) {
    browserLog->Write("hot-browser", "Native loginSuccess callback entered");
    originalLoginSuccess(self, unused);
}
unsigned* __fastcall Script(void*, void*, unsigned* output) {
    browserLog->Write("hot-browser", "Native bootstrap script requested");
    output[0] = output[1] = output[2] = 0;
    assign(output, nullptr, script.c_str(), 0xffffffffu, 0);
    return output;
}
int __fastcall ExtractFile(void* self, void* unused, const wchar_t* name, const unsigned short* reference,
                           const wchar_t* target, unsigned force, wchar_t* output, unsigned capacity) {
    const auto* file = FindHotBrowserFile(name, reference);
    if (file) {
        std::string label;
        WideToUtf8(file->name, label);
        browserLog->Write("hot-browser", "Native middleware requested: " + label);
    }
    if (!file)
        return originalExtract(self, unused, name, reference, target, force, output, capacity);
    if (!output || !capacity || capacity > MAX_PATH)
        return 0;
    const std::wstring path = directory + L"\\" + file->name;
    if (path.size() >= capacity) {
        output[0] = 0;
        return 0;
    }
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &info) ||
        (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || info.nFileSizeHigh ||
        info.nFileSizeLow != file->size) {
        output[0] = 0;
        return 0;
    }
    std::wmemcpy(output, path.c_str(), path.size() + 1);
    return 1;
}
} // namespace
const std::vector<HotBrowserFile>& HotBrowserFiles() {
    // Compatible middleware and bootstrap, explicitly not original HoT revisions.
    static const std::vector<HotBrowserFile> files = {
        {L"CoherentUI_Host.exe", 640203, 36143096, {0x64, 0xcc, 0xf3, 0x64, 0x0a, 0x21, 0x94, 0x25, 0x15, 0xba, 0x69,
                                                    0x7a, 0xd7, 0xe9, 0x0d, 0xe1, 0x97, 0x46, 0xcc, 0xfb, 0x02, 0x63,
                                                    0xae, 0xf1, 0xff, 0xb2, 0xf8, 0x4a, 0x5a, 0x51, 0xb0, 0xa7}},
        {L"CoherentUI.dll", 640202, 849912, {0x8e, 0xd1, 0x9f, 0x7a, 0xae, 0x3e, 0xab, 0x84, 0x34, 0xb2, 0xc0,
                                             0xea, 0x88, 0x3a, 0xd7, 0x3b, 0x28, 0xcb, 0x64, 0x1e, 0x21, 0x28,
                                             0x27, 0x7b, 0xb2, 0x4a, 0xef, 0x96, 0x74, 0x58, 0xe7, 0x13}},
        {L"icudt.dll", 640204, 9962488, {0x09, 0x9f, 0x23, 0xda, 0x03, 0x04, 0x8a, 0x60, 0xa0, 0x3d, 0x0e,
                                         0x6d, 0xf1, 0x49, 0x26, 0x18, 0xdc, 0xdf, 0xb3, 0xff, 0xca, 0xe1,
                                         0xc0, 0xd4, 0x62, 0x6e, 0xa9, 0x17, 0xc2, 0x2f, 0x38, 0x83}},
        {L"libEGL.dll", 640205, 110072, {0x77, 0xbd, 0xf3, 0x14, 0x25, 0xca, 0x81, 0x4f, 0x58, 0x0a, 0x0e,
                                         0x4e, 0xa6, 0xec, 0x3a, 0xc9, 0x8b, 0x3e, 0x73, 0xb7, 0x7c, 0x0b,
                                         0x42, 0xea, 0xec, 0x28, 0xfe, 0xdb, 0x43, 0x7a, 0xa1, 0x79}},
        {L"libGLESv2.dll", 640206, 890360, {0x98, 0x4b, 0x4d, 0xcb, 0x2f, 0x3d, 0x5b, 0x61, 0x2b, 0x75, 0x5b,
                                            0x97, 0x35, 0x37, 0xbf, 0xd5, 0xdd, 0xd6, 0xf3, 0x90, 0x4a, 0x94,
                                            0x45, 0x3b, 0x71, 0xa0, 0xcd, 0xfb, 0xe3, 0xaa, 0x56, 0x0d}},
        {L"d3dcompiler_43.dll", 720546, 2106216, {0x2f, 0x23, 0x18, 0x2e, 0xc6, 0xf4, 0x88, 0x93, 0x97, 0xac, 0x4b,
                                                  0xf0, 0x3d, 0x62, 0x53, 0x61, 0x36, 0xc5, 0xbd, 0xba, 0x82, 0x5c,
                                                  0x7d, 0x2c, 0x4e, 0xf0, 0x8c, 0x82, 0x7f, 0x3a, 0x8a, 0x1c}},
        {L"ffmpegsumo.dll", 791411, 867832, {0xba, 0x22, 0xb8, 0xbd, 0x05, 0xfc, 0x84, 0x99, 0x53, 0xa1, 0x1a,
                                             0xaf, 0x0b, 0x69, 0xa3, 0x46, 0x09, 0xf1, 0xc3, 0x21, 0x92, 0xca,
                                             0x1f, 0x0d, 0xbf, 0xe6, 0xf0, 0x5c, 0xe4, 0x67, 0xa7, 0x4c}},
        {L"browser-bootstrap.js", 0, 25201, {0xe0, 0x35, 0xcb, 0xf4, 0xc1, 0xac, 0x24, 0x0f, 0xbc, 0xe3, 0x54,
                                             0x75, 0x04, 0x46, 0x30, 0xd7, 0x08, 0x3d, 0xc7, 0x90, 0x61, 0x0e,
                                             0x38, 0xcf, 0xa2, 0x6e, 0x4d, 0x3c, 0x97, 0x65, 0xbc, 0xdb}}};
    return files;
}
const HotBrowserFile* FindHotBrowserFile(const wchar_t* name, const unsigned short* reference) {
    if (!name || !reference || reference[0] < 256 || reference[1] < 256 || reference[2])
        return nullptr;
    const uint32_t base = 0xff00u * (reference[1] - 256u) + (reference[0] - 256u) + 1u;
    for (const auto& file : HotBrowserFiles())
        if (file.baseId && file.baseId == base && !std::wcscmp(name, file.name))
            return &file;
    return nullptr;
}
bool LoadHotBrowser(const std::wstring& path, std::wstring& out, std::string& error) {
    std::wstring candidate;
    for (const auto& file : HotBrowserFiles()) {
        Buffer bytes;
        uint8_t hash[32];
        std::string name;
        WideToUtf8(file.name, name);
        error = "Missing or changed HoT browser resource: " + name;
        if (!ReadFileBytes(path + L"/" + file.name, file.size, bytes) || bytes.Size() != file.size ||
            !Sha256(bytes.View(), hash) || std::memcmp(hash, file.hash, sizeof(hash)))
            return false;
        if (!file.baseId &&
            !Utf8ToWide(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()), candidate))
            return false;
    }
    if (candidate.empty() || candidate.find(L'\0') != std::wstring::npos)
        return false;
    out = std::move(candidate);
    error.clear();
    return true;
}
const std::vector<HotSymbol>& HotBrowserSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {
        SYMBOL(hot_observe_wideStringAssign), SYMBOL(hot_observe_browserScript), SYMBOL(hot_observe_browserExtract),
        SYMBOL(hot_observe_loginSuccess)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotBrowserHooks() {
    static const std::vector<HookSpec> hooks = {
        {HotBrowserSymbols()[1].name, HotBrowserSymbols()[1].symbol, reinterpret_cast<void*>(Script), &originalScript,
         true},
        {HotBrowserSymbols()[2].name, HotBrowserSymbols()[2].symbol, reinterpret_cast<void*>(ExtractFile),
         reinterpret_cast<void**>(&originalExtract), true},
        {HotBrowserSymbols()[3].name, HotBrowserSymbols()[3].symbol, reinterpret_cast<void*>(ObserveLoginSuccess),
         reinterpret_cast<void**>(&originalLoginSuccess), true}};
    return hooks;
}
SymbolReference HotLauncherUrlSignature() {
    return {"launcher.url"};
}
bool PrepareHotBrowser(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT browser requires an unprepared x86 build 49284 client";
    if (prepared || !module || sizeof(void*) != 4 || image.Machine() != Bitness::X86 || runtime.config.build != 49284)
        return false;
    uint32_t assignRva = 0;
    for (size_t i = 0; i < HotBrowserSymbols().size(); ++i) {
        const auto& symbol = HotBrowserSymbols()[i];
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("HoT browser RVA is not verified: ") + symbol.name;
            return false;
        }
        if (!i)
            assignRva = match.rva;
    }
    const std::wstring privateDirectory = runtime.config.runtimeDirectory + L"\\bin";
    if (!LoadHotBrowser(privateDirectory, script, error))
        return false;
    const auto navigation = ResolveSymbol(image, HotLauncherUrlSignature());
    if (navigation.count != 1 || !runtime.config.webPort || runtime.config.webPort > 65535) {
        error = "Cannot verify HoT launcher navigation";
        return false;
    }
    if (!runtime.launcherUrl.ApplyPush32(
            static_cast<uint8_t*>(module) + navigation.rva + 10, L"coui://web/launcher/arena.html",
            L"http://127.0.0.1:" + std::to_wstring(runtime.config.webPort) + L"/launcher/49284/login", error))
        return false;
    assign = reinterpret_cast<Assign>(static_cast<uint8_t*>(module) + assignRva);
    browserLog = &runtime.log;
    directory = privateDirectory;
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
