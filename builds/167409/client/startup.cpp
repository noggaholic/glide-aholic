#include "startup.h"
#include "asset_hooks.h"
#include "glide/crypto.h"
#include "glide/files.h"
#include "glide/tls.h"
#include "../../204489/client/startup.h"
#include <Windows.h>
#include <cstring>
#include <cwchar>
#include <algorithm>
#ifdef _WIN64
#include "../../../vendor/minhook/src/hde/hde64.h"
#endif

namespace glide {
namespace china167409 {
namespace {
using Reserve = void(__fastcall*)(void*, unsigned);
using Tls = unsigned long long(__fastcall*)(unsigned char*, void*);
using Cipher = void(__fastcall*)(void*, unsigned, const void*);
using Local = void*(__fastcall*)(unsigned, const wchar_t*);
using Main = void*(__fastcall*)(unsigned);
using User = void(__fastcall*)(unsigned, wchar_t*, unsigned);
using Extract = unsigned long long(__fastcall*)(void*, void*, void*);
using Context = void*(__fastcall*)(void*, void*);
using Convert = void(__fastcall*)(const void*, void*);
using Patch = void(__fastcall*)(void*, unsigned);
using Analysis = void(__fastcall*)(void*, void*);
Reserve reserve = nullptr;
Convert convert = nullptr;
Tls originalTls = nullptr;
Cipher originalCipher = nullptr;
Local originalLocal = nullptr;
Main originalMain = nullptr;
User originalUser = nullptr;
Extract originalExtract = nullptr;
Context originalContext = nullptr;
Patch originalPatch = nullptr;
Analysis originalAnalysis = nullptr;
decltype(&CreateMutexA) originalCreateMutex = nullptr;
decltype(&OpenMutexA) originalOpenMutex = nullptr;
std::wstring privateRoot, localPath, documents, cefRoot;
std::string mutexSuffix;
Buffer contextScript;
Log* logger = nullptr;
unsigned long long __fastcall ServerHello(unsigned char* client, void* reader) {
    const auto session = LocalTlsSessionId(), secret = LocalTlsMasterSecret();
    *reinterpret_cast<unsigned*>(client + 0x4f0c) = 0;
    reserve(client + 0x4ef8, static_cast<unsigned>(session.size));
    std::memcpy(*reinterpret_cast<void**>(client + 0x4f00), session.data, session.size);
    *reinterpret_cast<unsigned*>(client + 0x4f0c) = static_cast<unsigned>(session.size);
    std::memcpy(client + 0x88, secret.data, secret.size);
    return originalTls(client, reader);
}
void __fastcall CipherSetup(void* state, unsigned, const void*) {
    InitializeBuild204489Cipher(state);
}
void* __fastcall LocalArchive(unsigned flags, const wchar_t*) {
    return originalLocal(flags & ~8u, localPath.c_str());
}
void* __fastcall MainArchive(unsigned flags) {
    return originalMain(flags | 8u);
}
void __fastcall UserPath(unsigned kind, wchar_t* output, unsigned capacity) {
    if (kind != 1 && kind != 2) {
        originalUser(kind, output, capacity);
        return;
    }
    const auto& path = kind == 1 ? privateRoot : documents;
    if (!output || capacity <= path.size()) {
        TerminateProcess(GetCurrentProcess(), ERROR_INSUFFICIENT_BUFFER);
        return;
    }
    std::memcpy(output, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
}
const char* Scoped(const char* name, std::string& result) {
    if (!name || std::strncmp(name, "AN-Mutex-Window-", 16))
        return name;
    result = name;
    result += mutexSuffix;
    return result.c_str();
}
HANDLE WINAPI CreateScoped(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name) {
    std::string scoped;
    return originalCreateMutex(attributes, owner, Scoped(name, scoped));
}
HANDLE WINAPI OpenScoped(DWORD access, BOOL inherit, LPCSTR name) {
    std::string scoped;
    return originalOpenMutex(access, inherit, Scoped(name, scoped));
}
unsigned long long __fastcall CefExtract(void* filename, void* asset, void* version) {
    // Preserve the existing local-file path; private CEF files were checked by the launcher.
    if (filename && *reinterpret_cast<int*>(static_cast<uint8_t*>(filename) + 8)) {
        const auto* path = *static_cast<const wchar_t**>(filename);
        if (path && !_wcsnicmp(path, cefRoot.c_str(), cefRoot.size()) && path[cefRoot.size()] &&
            !std::wcsstr(path + cefRoot.size(), L"..")) {
            const DWORD attributes = GetFileAttributesW(path);
            if (attributes != INVALID_FILE_ATTRIBUTES &&
                !(attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
                return 1;
        }
    }
    return originalExtract(filename, asset, version);
}
void* __fastcall CefContext(void* context, void* result) {
    if (!result)
        return originalContext(context, result);
    struct Source {
        const char* data;
        unsigned count, reserved;
    };
    const Source source = {reinterpret_cast<const char*>(contextScript.Data()),
                           static_cast<unsigned>(contextScript.Size()), 0};
    std::memset(result, 0, 24);
    static_cast<unsigned long long*>(result)[2] = 1;
    convert(&source, result);
    return result;
}
void __fastcall PatchState(void* context, unsigned state) {
    // The existing China implementation skips only trim; the derived root still loads normally.
    if (state == 7)
        SourceContentReady();
    originalPatch(context, state == 3 ? 4 : state);
}
void __fastcall ArchiveAnalysis(void*, void*) {
    if (logger)
        logger->Write("china", "Using the validated original chunk archive without updater analysis");
}
} // namespace
const std::vector<HookSpec>& StartupHooks() {
    static const std::vector<HookSpec> result = [] {
        const auto& symbols = StartupSymbols();
        std::vector<HookSpec> hooks;
        const size_t indices[] = {1, 2, 3, 4, 5, 6, 7, 9, 10};
        void* detours[] = {reinterpret_cast<void*>(ServerHello),    reinterpret_cast<void*>(CipherSetup),
                           reinterpret_cast<void*>(LocalArchive),   reinterpret_cast<void*>(MainArchive),
                           reinterpret_cast<void*>(UserPath),       reinterpret_cast<void*>(CefExtract),
                           reinterpret_cast<void*>(CefContext),     reinterpret_cast<void*>(PatchState),
                           reinterpret_cast<void*>(ArchiveAnalysis)};
        void** originals[] = {reinterpret_cast<void**>(&originalTls),     reinterpret_cast<void**>(&originalCipher),
                              reinterpret_cast<void**>(&originalLocal),   reinterpret_cast<void**>(&originalMain),
                              reinterpret_cast<void**>(&originalUser),    reinterpret_cast<void**>(&originalExtract),
                              reinterpret_cast<void**>(&originalContext), reinterpret_cast<void**>(&originalPatch),
                              reinterpret_cast<void**>(&originalAnalysis)};
        for (size_t i = 0; i < sizeof(indices) / sizeof(indices[0]); ++i) {
            const auto& symbol = symbols[indices[i]];
            hooks.push_back({symbol.name, symbol.symbol, detours[i], originals[i], true});
        }
        return hooks;
    }();
    return result;
}
bool PrepareStartup(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "China startup requires its original x64 build";
#ifdef _WIN64
    if (runtime.config.build != 167409 || image.Machine() != Bitness::X64 || !module)
        return false;
    const auto& symbols = StartupSymbols();
    std::vector<uint32_t> addresses;
    for (const auto& symbol : symbols) {
        const auto found = ResolveSymbol(image, symbol.symbol);
        if (found.count != 1) {
            error = std::string("China startup pattern is not unique: ") + symbol.name;
            return false;
        }
        addresses.push_back(found.rva);
    }
    Buffer bytes;
    uint8_t digest[32];
    const uint8_t expected[] = {0x79, 0xc3, 0x35, 0xfd, 0xdf, 0x52, 0x5c, 0x85, 0x9d, 0x98, 0xde,
                                0x62, 0xe3, 0x90, 0x55, 0x4c, 0x02, 0x0c, 0xf2, 0x48, 0x00, 0x9e,
                                0xbf, 0x95, 0x26, 0x64, 0xdc, 0xd0, 0x04, 0x21, 0x5d, 0xf3};
    error = "Pinned original China CEF context script is missing or changed";
    if (!ReadFileBytes(runtime.config.runtimeDirectory + L"\\GlideAssets\\cef-context-918051.raw", 21627, bytes) ||
        bytes.Size() != 21627 || !Sha256(bytes.View(), digest) || std::memcmp(digest, expected, sizeof(expected)) ||
        !bytes.U8(0))
        return false;
    privateRoot = runtime.config.runtimeDirectory;
    localPath = privateRoot + L"\\Local.dat";
    documents = privateRoot + L"\\Documents";
    cefRoot = privateRoot + L"\\bin64\\cef\\";
    error = "China private runtime paths exceed the native capacity";
    if (documents.size() >= MAX_PATH || localPath.size() >= MAX_PATH || cefRoot.size() >= MAX_PATH)
        return false;
    const DWORD attributes = GetFileAttributesW(documents.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT))
        return false;
    reserve = reinterpret_cast<Reserve>(static_cast<uint8_t*>(module) + addresses[0]);
    convert = reinterpret_cast<Convert>(static_cast<uint8_t*>(module) + addresses[8]);
    contextScript = std::move(bytes);
    logger = &runtime.log;
    mutexSuffix = "-GlideAholic-167409-" + std::to_string(GetCurrentProcessId());
    return runtime.hooks.AttachExport(L"kernel32.dll", "CreateMutexA", reinterpret_cast<void*>(CreateScoped),
                                      reinterpret_cast<void**>(&originalCreateMutex), error) &&
           runtime.hooks.AttachExport(L"kernel32.dll", "OpenMutexA", reinterpret_cast<void*>(OpenScoped),
                                      reinterpret_cast<void**>(&originalOpenMutex), error);
#else
    (void)runtime;
    (void)image;
    (void)module;
    return false;
#endif
}
bool PrepareLauncherUrl(RelativeStringPatch& patch, const PeImage& image, void* module, unsigned webPort,
                        std::string& error) {
    error = "Cannot verify the original China launcher URL instruction";
#ifdef _WIN64
    if (image.Machine() != Bitness::X64 || !module || !webPort || webPort > 65535)
        return false;
    const auto match = ResolveSymbol(image, LauncherUrlSignature());
    if (match.count != 1)
        return false;
    auto* instruction = static_cast<uint8_t*>(module) + match.rva;
    DWORD64 base = 0;
    const auto* function = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(instruction), &base, nullptr);
    if (!function || base != reinterpret_cast<DWORD64>(module) || function->BeginAddress > match.rva ||
        function->EndAddress < match.rva + 7)
        return false;
    uint32_t at = function->BeginAddress;
    while (at < match.rva) {
        uint8_t bytes[16] = {};
        SIZE_T read = 0;
        const size_t count = (std::min)(size_t(15), static_cast<size_t>(function->EndAddress - at));
        if (!ReadProcessMemory(GetCurrentProcess(), static_cast<uint8_t*>(module) + at, bytes, count, &read) ||
            read != count)
            return false;
        hde64s decoded = {};
        const unsigned size = hde64_disasm(bytes, &decoded);
        if (!size || size > count || (decoded.flags & F_ERROR))
            return false;
        at += size;
    }
    if (at != match.rva)
        return false;
    return patch.Apply(instruction, L"coui://web/launcher/kongzhong/index.html",
                       L"http://127.0.0.1:" + std::to_wstring(webPort) + L"/launcher/167409/login", error);
#else
    (void)patch;
    (void)image;
    (void)module;
    (void)webPort;
    return false;
#endif
}
} // namespace china167409
} // namespace glide
