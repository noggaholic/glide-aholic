#include "startup.h"
#include "glide/tls.h"
#include <Windows.h>
#include <cstring>
#include <cwchar>
#include <algorithm>
#ifdef _WIN64
#include "../../../vendor/minhook/src/hde/hde64.h"
#endif

namespace glide {
namespace {

std::wstring localArchive, userData, documents;
std::string mutexSuffix;
using Reserve = void(__fastcall*)(void*, unsigned);
using TlsHello = unsigned long long(__fastcall*)(unsigned char*, void*);
using Cipher = void(__fastcall*)(void*, unsigned, const void*);
using LocalArchive = void*(__fastcall*)(unsigned, const wchar_t*);
using MainArchive = void*(__fastcall*)(unsigned);
using UserData = void(__fastcall*)(unsigned, wchar_t*, unsigned);
Reserve reserveArray = nullptr;
TlsHello originalTls = nullptr;
Cipher originalCipher = nullptr;
LocalArchive originalLocal = nullptr;
MainArchive originalMain = nullptr;
UserData originalUser = nullptr;
decltype(&CreateMutexA) originalCreateMutex = nullptr;
decltype(&OpenMutexA) originalOpenMutex = nullptr;
unsigned long long __fastcall Tls(unsigned char* client, void* reader) {
    const auto session = LocalTlsSessionId(), secret = LocalTlsMasterSecret();
    *reinterpret_cast<unsigned*>(client + 0x4f0c) = 0;
    reserveArray(client + 0x4ef8, static_cast<unsigned>(session.size));
    std::memcpy(*reinterpret_cast<void**>(client + 0x4f00), session.data, session.size);
    *reinterpret_cast<unsigned*>(client + 0x4f0c) = static_cast<unsigned>(session.size);
    std::memcpy(client + 0x88, secret.data, secret.size);
    return originalTls(client, reader);
}
void __fastcall CipherSetup(void* state, unsigned, const void*) {
    InitializeBuild204489Cipher(state);
}
void* __fastcall OpenLocal(unsigned flags, const wchar_t*) {
    return originalLocal(flags & ~8u, localArchive.c_str());
}
void* __fastcall OpenMain(unsigned flags) {
    return originalMain(flags | 8u);
}
bool UseOriginalArchive(uint8_t* base, uint32_t branchRva, uint32_t targetRva) {
#ifndef _WIN64
    (void)base;
    (void)branchRva;
    (void)targetRva;
    return false;
#else
    auto* branch = base + branchRva;
    // Accept only a short conditional branch to the measured destination.
    // The exact file identity and both section-checked sites were resolved first.
    hde64s instruction{};
    if (hde64_disasm(branch, &instruction) != 2 || instruction.flags & F_ERROR || instruction.opcode < 0x70 ||
        instruction.opcode > 0x7f || int64_t(branchRva) + 2 + static_cast<int8_t>(instruction.imm.imm8) != targetRva)
        return false;
    DWORD previous = 0;
    if (!VirtualProtect(branch, 2, PAGE_EXECUTE_READWRITE, &previous))
        return false;
    const uint8_t jump = 0xeb;
    SIZE_T written = 0;
    const bool changed = WriteProcessMemory(GetCurrentProcess(), branch, &jump, 1, &written) && written == 1;
    DWORD ignored = 0;
    const bool restored = VirtualProtect(branch, 2, previous, &ignored) != FALSE;
    return changed && restored && FlushInstructionCache(GetCurrentProcess(), branch, 2);
#endif
}
void __fastcall UserPath(unsigned kind, wchar_t* output, unsigned capacity) {
    if (kind != 1 && kind != 2) {
        originalUser(kind, output, capacity);
        return;
    }
    const auto& path = kind == 1 ? userData : documents;
    if (!output || capacity <= path.size()) {
        TerminateProcess(GetCurrentProcess(), ERROR_INSUFFICIENT_BUFFER);
        return;
    }
    std::memcpy(output, path.c_str(), (path.size() + 1) * sizeof(wchar_t));
}
const char* Scoped(const char* name, std::string& buffer) {
    if (!name || std::strncmp(name, "AN-Mutex-Window-", 16))
        return name;
    buffer = name;
    buffer += mutexSuffix;
    return buffer.c_str();
}
HANDLE WINAPI CreateScoped(LPSECURITY_ATTRIBUTES attributes, BOOL owner, LPCSTR name) {
    std::string scoped;
    return originalCreateMutex(attributes, owner, Scoped(name, scoped));
}
HANDLE WINAPI OpenScoped(DWORD access, BOOL inherit, LPCSTR name) {
    std::string scoped;
    return originalOpenMutex(access, inherit, Scoped(name, scoped));
}
} // namespace
const std::vector<StartupSymbol>& Build204489StartupSymbols() {
    static const std::vector<StartupSymbol> symbols = {{"ArrayReserve", {"ArrayReserve"}},
                                                       {"TlsServerHello", {"TlsServerHello"}},
                                                       {"Rc4Ksa", {"Rc4Ksa"}},
                                                       {"LocalArchive", {"LocalArchive"}},
                                                       {"MainArchive", {"MainArchive"}},
                                                       {"UserDataPath", {"UserDataPath"}},
                                                       {"ArchiveAnalysis", {"ArchiveAnalysis"}}};
    return symbols;
}
SymbolReference Build204489LauncherUrlSignature() {
    return {"launcher.url"};
}
bool PrepareBuild204489LauncherUrl(RelativeStringPatch& patch, const PeImage& image, void* module, unsigned webPort,
                                   std::string& error) {
    error = "Cannot verify build 204489 launcher URL instruction";
#ifdef _WIN64
    if (image.Machine() != Bitness::X64 || !module || !webPort || webPort > 65535)
        return false;
    const auto match = ResolveSymbol(image, Build204489LauncherUrlSignature());
    if (match.count != 1)
        return false;
    auto* instruction = static_cast<uint8_t*>(module) + match.rva + 15;
    DWORD64 base = 0;
    const auto* function = RtlLookupFunctionEntry(reinterpret_cast<DWORD64>(instruction), &base, nullptr);
    if (!function || base != reinterpret_cast<DWORD64>(module) || function->BeginAddress > match.rva ||
        function->EndAddress < match.rva + 22)
        return false;
    // Decode from the PE unwind entry so a byte-pattern match cannot start inside another instruction.
    uint32_t offset = function->BeginAddress;
    while (offset < match.rva + 15) {
        uint8_t bytes[16] = {};
        SIZE_T read = 0;
        const size_t count = (std::min)(size_t(15), static_cast<size_t>(function->EndAddress - offset));
        if (!ReadProcessMemory(GetCurrentProcess(), static_cast<uint8_t*>(module) + offset, bytes, count, &read) ||
            read != count)
            return false;
        hde64s decoded = {};
        const unsigned size = hde64_disasm(bytes, &decoded);
        if (!size || size > count || (decoded.flags & F_ERROR))
            return false;
        offset += size;
    }
    if (offset != match.rva + 15)
        return false;
    return patch.Apply(instruction, L"coui://web/launcher/arenanet/index.html",
                       L"http://127.0.0.1:" + std::to_wstring(webPort) + L"/launcher/204489/login", error);
#else
    (void)patch;
    (void)image;
    (void)module;
    (void)webPort;
    return false;
#endif
}
void InitializeBuild204489Cipher(void* state) {
    const uint8_t key[] = "GLIDE_AHOLIC_RC4_KEY";
    auto* bytes = static_cast<uint8_t*>(state);
    std::memset(bytes, 0, 8);
    for (unsigned i = 0; i < 256; ++i)
        bytes[8 + i] = static_cast<uint8_t>(i);
    unsigned j = 0;
    for (unsigned i = 0; i < 256; ++i) {
        j = (j + bytes[8 + i] + key[i % (sizeof(key) - 1)]) & 255;
        std::swap(bytes[8 + i], bytes[8 + j]);
    }
}
const std::vector<HookSpec>& Build204489StartupHooks() {
    const auto& symbols = Build204489StartupSymbols();
    void* detours[] = {reinterpret_cast<void*>(Tls), reinterpret_cast<void*>(CipherSetup),
                       reinterpret_cast<void*>(OpenLocal), reinterpret_cast<void*>(OpenMain),
                       reinterpret_cast<void*>(UserPath)};
    void** originals[] = {reinterpret_cast<void**>(&originalTls), reinterpret_cast<void**>(&originalCipher),
                          reinterpret_cast<void**>(&originalLocal), reinterpret_cast<void**>(&originalMain),
                          reinterpret_cast<void**>(&originalUser)};
    static const std::vector<HookSpec> hooks = [&] {
        std::vector<HookSpec> result;
        for (size_t i = 0; i < sizeof(detours) / sizeof(detours[0]); ++i)
            result.push_back({symbols[i + 1].name, symbols[i + 1].symbol, detours[i], originals[i], true});
        return result;
    }();
    return hooks;
}
bool PrepareBuild204489Startup(Hooks& hooks, const PeImage& image, void* module, const SlotConfig& config,
                               std::string& error) {
    error = "Build 204489 startup requires the 204489 x64 client";
    if (config.build != 204489 || image.Machine() != Bitness::X64 || sizeof(void*) != 8 || !module)
        return false;
    const auto& symbols = Build204489StartupSymbols();
    std::vector<uint32_t> addresses;
    for (const auto& symbol : symbols) {
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("Required build 204489 startup RVA is not verified: ") + symbol.name;
            return false;
        }
        addresses.push_back(match.rva);
    }
    userData = config.runtimeDirectory;
    localArchive = userData + L"\\Local.dat";
    documents = userData + L"\\Documents";
    error = "Private runtime paths exceed native client limits";
    if (documents.size() >= MAX_PATH || localArchive.size() >= MAX_PATH)
        return false;
    // The launcher owns and prepares these directories before injection.
    const DWORD attributes = GetFileAttributesW(documents.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || !(attributes & FILE_ATTRIBUTE_DIRECTORY) ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
        error = "Private Documents directory is missing or invalid";
        return false;
    }
    mutexSuffix = "-GlideAholic-" + std::to_string(config.build) + "-" + std::to_string(GetCurrentProcessId());
    reserveArray = reinterpret_cast<Reserve>(static_cast<uint8_t*>(module) + addresses[0]);
    error = "Cannot verify build 204489 offline archive branch";
    const auto branch = ResolveSymbol(image, {"archive.updater.branch"});
    const auto destination = ResolveSymbol(image, {"archive.updater.target"});
    if (branch.count != 1 || destination.count != 1 ||
        !UseOriginalArchive(static_cast<uint8_t*>(module), branch.rva, destination.rva))
        return false;
    return hooks.AttachExport(L"kernel32.dll", "CreateMutexA", reinterpret_cast<void*>(CreateScoped),
                              reinterpret_cast<void**>(&originalCreateMutex), error) &&
           hooks.AttachExport(L"kernel32.dll", "OpenMutexA", reinterpret_cast<void*>(OpenScoped),
                              reinterpret_cast<void**>(&originalOpenMutex), error);
}
} // namespace glide
