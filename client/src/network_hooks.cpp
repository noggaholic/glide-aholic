#include "network_hooks.h"
#include <ws2tcpip.h>
#include <mswsock.h>
#include <windns.h>
#include <iphlpapi.h>
#include <atomic>
#include <cstring>
#include <cstdio>

namespace glide {
namespace {
NetworkPolicy policy;
Log* networkLog = nullptr;
void HostLog(const char* api, const char* node) {
    if (networkLog)
        networkLog->Write("network",
                          std::string(api) + " host=" + (node ? std::string(node, strnlen(node, 255)) : "(null)"));
}
decltype(&connect) originalConnect = nullptr;
decltype(&WSAConnect) originalWsaConnect = nullptr;
decltype(&getaddrinfo) originalAddress = nullptr;
decltype(&GetAddrInfoW) originalAddressWide = nullptr;
decltype(&WSAIoctl) originalIoctl = nullptr;
std::atomic<LPFN_CONNECTEX> originalConnectEx{nullptr};
void* blockedOriginals[10] = {};
LONG nextLookup = 0x40000000;
struct LocalHost {
    hostent host;
    char* aliases[1];
    char* addresses[2];
    ULONG address;
    char name[256];
};
thread_local LocalHost localHost;
int Denied() {
    WSASetLastError(WSAEACCES);
    return SOCKET_ERROR;
}
bool MachineAddress(in_addr& address) {
    ULONG size = 16384;
    for (unsigned attempt = 0; attempt < 3 && size <= 1024 * 1024; ++attempt) {
        std::vector<uint8_t> storage(size);
        auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(storage.data());
        const ULONG result =
            GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                 nullptr, adapters, &size);
        if (result == ERROR_BUFFER_OVERFLOW)
            continue;
        if (result != NO_ERROR)
            return false;
        for (auto* adapter = adapters; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            for (auto* entry = adapter->FirstUnicastAddress; entry; entry = entry->Next) {
                if (entry->Address.iSockaddrLength < static_cast<int>(sizeof(sockaddr_in)))
                    continue;
                const auto* ip = reinterpret_cast<const sockaddr_in*>(entry->Address.lpSockaddr);
                if (!ip || ip->sin_family != AF_INET)
                    continue;
                const ULONG numeric = ntohl(ip->sin_addr.s_addr);
                if (numeric && (numeric >> 24) != 127 && (numeric >> 16) != 0xa9fe) {
                    address = ip->sin_addr;
                    return true;
                }
            }
        }
        return false;
    }
    return false;
}
bool Route(const sockaddr* address, int size, sockaddr_in& target) {
    if (policy.Route(address, size, target)) {
        if (networkLog)
            networkLog->Write("network", "Connect redirected port=" + std::to_string(ntohs(target.sin_port)));
        return true;
    }
    if (networkLog)
        networkLog->Write("network", "Connect denied by configured route policy");
    Denied();
    return false;
}
int WSAAPI GuardConnect(SOCKET socket, const sockaddr* address, int size) {
    sockaddr_in target;
    return Route(address, size, target)
               ? originalConnect(socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target))
               : SOCKET_ERROR;
}
int WSAAPI GuardWsaConnect(SOCKET socket, const sockaddr* address, int size, LPWSABUF caller, LPWSABUF callee,
                           LPQOS sqos, LPQOS gqos) {
    sockaddr_in target;
    return Route(address, size, target) ? originalWsaConnect(socket, reinterpret_cast<const sockaddr*>(&target),
                                                             sizeof(target), caller, callee, sqos, gqos)
                                        : SOCKET_ERROR;
}
BOOL PASCAL GuardConnectEx(SOCKET socket, const sockaddr* address, int size, PVOID buffer, DWORD length, LPDWORD sent,
                           LPOVERLAPPED overlap) {
    sockaddr_in target;
    const auto original = originalConnectEx.load();
    if (!original || !Route(address, size, target)) {
        Denied();
        return FALSE;
    }
    return original(socket, reinterpret_cast<const sockaddr*>(&target), sizeof(target), buffer, length, sent, overlap);
}
int WSAAPI GuardIoctl(SOCKET socket, DWORD code, LPVOID input, DWORD inputSize, LPVOID output, DWORD outputSize,
                      LPDWORD returned, LPWSAOVERLAPPED overlap, LPWSAOVERLAPPED_COMPLETION_ROUTINE callback) {
    if (code == SIO_GET_EXTENSION_FUNCTION_POINTER) {
        const GUID expected = WSAID_CONNECTEX;
        if (!input || inputSize != sizeof(expected) || std::memcmp(input, &expected, sizeof(expected)) || overlap ||
            callback || !output || outputSize < sizeof(LPFN_CONNECTEX))
            return Denied();
        const int result =
            originalIoctl(socket, code, input, inputSize, output, outputSize, returned, overlap, callback);
        if (!result) {
            const auto function = *static_cast<LPFN_CONNECTEX*>(output);
            auto previous = originalConnectEx.load();
            if (!previous)
                originalConnectEx.compare_exchange_strong(previous, function);
            if (previous && previous != function) {
                *static_cast<LPFN_CONNECTEX*>(output) = nullptr;
                return Denied();
            }
            *static_cast<LPFN_CONNECTEX*>(output) = GuardConnectEx;
        }
        return result;
    }
    return originalIoctl(socket, code, input, inputSize, output, outputSize, returned, overlap, callback);
}
int WSAAPI GuardAddress(PCSTR node, PCSTR service, const ADDRINFOA* hints, PADDRINFOA* result) {
    HostLog("getaddrinfo", node);
    if (result)
        *result = nullptr;
    if (!result)
        return EAI_FAIL;
    const auto kind = policy.Classify(node);
    if (kind == HostTarget::Denied) {
        WSASetLastError(WSAHOST_NOT_FOUND);
        return EAI_NONAME;
    }
    ADDRINFOA local = {};
    if (hints)
        local = *hints;
    if (local.ai_family != AF_UNSPEC && local.ai_family != AF_INET)
        return EAI_FAMILY;
    local.ai_family = AF_INET;
    local.ai_flags |= AI_NUMERICHOST;
    if (kind == HostTarget::Numeric)
        return originalAddress(node, service, &local, result);
    in_addr address = {};
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (kind == HostTarget::Machine && !MachineAddress(address))
        return EAI_NONAME;
    if (kind == HostTarget::Game)
        address = policy.ServerAddress();
    char numeric[INET_ADDRSTRLEN], port[8];
    if (!InetNtopA(AF_INET, &address, numeric, sizeof(numeric)))
        return EAI_FAIL;
    if (kind == HostTarget::Game) {
        std::snprintf(port, sizeof(port), "%u", policy.GamePort());
        service = port;
    }
    return originalAddress(kind == HostTarget::Passive ? nullptr : numeric, service, &local, result);
}
int WSAAPI GuardAddressWide(PCWSTR node, PCWSTR service, const ADDRINFOW* hints, PADDRINFOW* result) {
    if (result)
        *result = nullptr;
    if (!result)
        return EAI_FAIL;
    char host[256];
    if (node && !WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, node, -1, host, sizeof(host), nullptr, nullptr))
        return EAI_NONAME;
    const auto kind = policy.Classify(node ? host : nullptr);
    if (kind == HostTarget::Denied) {
        WSASetLastError(WSAHOST_NOT_FOUND);
        return EAI_NONAME;
    }
    ADDRINFOW local = {};
    if (hints)
        local = *hints;
    if (local.ai_family != AF_UNSPEC && local.ai_family != AF_INET)
        return EAI_FAMILY;
    local.ai_family = AF_INET;
    local.ai_flags |= AI_NUMERICHOST;
    if (kind == HostTarget::Numeric)
        return originalAddressWide(node, service, &local, result);
    in_addr address = {};
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (kind == HostTarget::Machine && !MachineAddress(address))
        return EAI_NONAME;
    if (kind == HostTarget::Game)
        address = policy.ServerAddress();
    wchar_t numeric[INET_ADDRSTRLEN], port[8];
    if (!InetNtopW(AF_INET, &address, numeric, INET_ADDRSTRLEN))
        return EAI_FAIL;
    if (kind == HostTarget::Game) {
        swprintf_s(port, 8, L"%u", policy.GamePort());
        service = port;
    }
    return originalAddressWide(kind == HostTarget::Passive ? nullptr : numeric, service, &local, result);
}
bool FillHost(LocalHost& local, const char* node) {
    const auto kind = policy.Classify(node);
    if (kind == HostTarget::Denied) {
        WSASetLastError(WSAHOST_NOT_FOUND);
        return false;
    }
    std::memset(&local, 0, sizeof(local));
    in_addr address = {};
    address.s_addr = htonl(INADDR_LOOPBACK);
    if (kind == HostTarget::Game)
        address = policy.ServerAddress();
    if (kind == HostTarget::Numeric && InetPtonA(AF_INET, node, &address) != 1)
        return false;
    if (kind == HostTarget::Machine && !MachineAddress(address)) {
        WSASetLastError(WSAHOST_NOT_FOUND);
        return false;
    }
    if (!InetNtopA(AF_INET, &address, local.name, sizeof(local.name)))
        return false;
    if (kind == HostTarget::Machine) {
        DWORD size = sizeof(local.name);
        GetComputerNameA(local.name, &size);
    }
    local.address = address.s_addr;
    local.addresses[0] = reinterpret_cast<char*>(&local.address);
    local.host.h_name = local.name;
    local.host.h_aliases = local.aliases;
    local.host.h_addrtype = AF_INET;
    local.host.h_length = 4;
    local.host.h_addr_list = local.addresses;
    return true;
}
hostent* WSAAPI GuardHost(const char* node) {
    HostLog("gethostbyname", node);
    return FillHost(localHost, node) ? &localHost.host : nullptr;
}
HANDLE WSAAPI GuardAsync(HWND window, u_int message, const char* node, char* buffer, int size) {
    HostLog("WSAAsyncGetHostByName", node);
    if (policy.Classify(node) == HostTarget::Denied) {
        WSASetLastError(WSAHOST_NOT_FOUND);
        return nullptr;
    }
    int error = 0;
    if (!buffer || size < static_cast<int>(sizeof(LocalHost)))
        error = WSAENOBUFS;
    else if (!FillHost(*reinterpret_cast<LocalHost*>(buffer), node))
        error = WSAHOST_NOT_FOUND;
    HANDLE request =
        reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(static_cast<ULONG>(InterlockedIncrement(&nextLookup))));
    if (!PostMessageW(window, message, reinterpret_cast<WPARAM>(request),
                      WSAMAKEASYNCREPLY(sizeof(LocalHost), error))) {
        WSASetLastError(WSAEINVAL);
        return nullptr;
    }
    return request;
}
INT WSAAPI BlockAddressWide(PCWSTR, PCWSTR, DWORD, LPGUID, const ADDRINFOEXW*, PADDRINFOEXW* result, timeval*,
                            LPOVERLAPPED, LPLOOKUPSERVICE_COMPLETION_ROUTINE, LPHANDLE handle) {
    if (result)
        *result = nullptr;
    if (handle)
        *handle = nullptr;
    return Denied();
}
INT WSAAPI BlockAddressAnsi(PCSTR, PCSTR, DWORD, LPGUID, const ADDRINFOEXA*, PADDRINFOEXA* result, timeval*,
                            LPOVERLAPPED, LPLOOKUPSERVICE_COMPLETION_ROUTINE, LPHANDLE handle) {
    if (result)
        *result = nullptr;
    if (handle)
        *handle = nullptr;
    return Denied();
}
DNS_STATUS WINAPI BlockDnsAnsi(PCSTR, WORD, DWORD, PVOID, PDNS_RECORD* result, PVOID*) {
    if (result)
        *result = nullptr;
    return DNS_ERROR_RCODE_NAME_ERROR;
}
DNS_STATUS WINAPI BlockDnsWide(PCWSTR, WORD, DWORD, PVOID, PDNS_RECORD* result, PVOID*) {
    if (result)
        *result = nullptr;
    return DNS_ERROR_RCODE_NAME_ERROR;
}
DNS_STATUS WINAPI BlockDnsEx(PVOID, PVOID, PVOID) {
    return DNS_ERROR_RCODE_NAME_ERROR;
}
int WSAAPI BlockSendTo(SOCKET, const char*, int, int, const sockaddr*, int) {
    return Denied();
}
int WSAAPI BlockWsaSendTo(SOCKET, LPWSABUF, DWORD, LPDWORD, DWORD, const sockaddr*, int, LPWSAOVERLAPPED,
                          LPWSAOVERLAPPED_COMPLETION_ROUTINE) {
    return Denied();
}
} // namespace
bool InstallNetworkHooks(Hooks& hooks, const NetworkPolicy& configured, std::string& error, Log* log) {
    error = "Cannot prepare network protection";
    if (!configured.GamePort() || originalConnect)
        return false;
    // Hook targets must remain loaded for the lifetime of this injected process.
    static const HMODULE winsockModule = LoadLibraryW(L"ws2_32.dll");
    static const HMODULE dnsModule = LoadLibraryW(L"dnsapi.dll");
    if (!winsockModule || !dnsModule)
        return false;
    policy = configured;
    networkLog = log;
    originalConnectEx.store(nullptr);
    struct Export {
        const wchar_t* library;
        const char* name;
        void* replacement;
        void** original;
    };
    const Export exports[] = {
        {L"ws2_32.dll", "connect", reinterpret_cast<void*>(GuardConnect), reinterpret_cast<void**>(&originalConnect)},
        {L"ws2_32.dll", "WSAConnect", reinterpret_cast<void*>(GuardWsaConnect),
         reinterpret_cast<void**>(&originalWsaConnect)},
        {L"ws2_32.dll", "getaddrinfo", reinterpret_cast<void*>(GuardAddress),
         reinterpret_cast<void**>(&originalAddress)},
        {L"ws2_32.dll", "GetAddrInfoW", reinterpret_cast<void*>(GuardAddressWide),
         reinterpret_cast<void**>(&originalAddressWide)},
        {L"ws2_32.dll", "WSAIoctl", reinterpret_cast<void*>(GuardIoctl), reinterpret_cast<void**>(&originalIoctl)},
        {L"ws2_32.dll", "gethostbyname", reinterpret_cast<void*>(GuardHost), &blockedOriginals[0]},
        {L"ws2_32.dll", "WSAAsyncGetHostByName", reinterpret_cast<void*>(GuardAsync), &blockedOriginals[1]},
        {L"ws2_32.dll", "GetAddrInfoExW", reinterpret_cast<void*>(BlockAddressWide), &blockedOriginals[2]},
        {L"ws2_32.dll", "GetAddrInfoExA", reinterpret_cast<void*>(BlockAddressAnsi), &blockedOriginals[3]},
        {L"ws2_32.dll", "sendto", reinterpret_cast<void*>(BlockSendTo), &blockedOriginals[4]},
        {L"ws2_32.dll", "WSASendTo", reinterpret_cast<void*>(BlockWsaSendTo), &blockedOriginals[5]},
        {L"dnsapi.dll", "DnsQuery_A", reinterpret_cast<void*>(BlockDnsAnsi), &blockedOriginals[6]},
        {L"dnsapi.dll", "DnsQuery_W", reinterpret_cast<void*>(BlockDnsWide), &blockedOriginals[7]},
        {L"dnsapi.dll", "DnsQuery_UTF8", reinterpret_cast<void*>(BlockDnsAnsi), &blockedOriginals[8]},
        {L"dnsapi.dll", "DnsQueryEx", reinterpret_cast<void*>(BlockDnsEx), &blockedOriginals[9]}};
    for (const auto& entry : exports)
        if (!hooks.AttachExport(entry.library, entry.name, entry.replacement, entry.original, error))
            return false;
    error.clear();
    return true;
}
} // namespace glide
