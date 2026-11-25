#include "transport.h"
#include "../../../client/src/runtime.h"
#include "glide/tls.h"
#include <cstring>
#include <algorithm>

namespace glide {
namespace {
Glide49284TlsConstructFn originalConstruct = nullptr;
Glide49284TlsSetSessionFn setSession = nullptr;
void* originalCipher = nullptr;
bool prepared = false;
void* __fastcall Construct(void* self, void*, uintptr_t callback, uintptr_t randomSource) {
    return ConstructHotLocalSession(originalConstruct, setSession, self, callback, randomSource);
}
void __fastcall Cipher(void* state, void*, unsigned, const void*) {
    InitializeHotCipher(state);
}
} // namespace
void* ConstructHotLocalSession(Glide49284TlsConstructFn construct, Glide49284TlsSetSessionFn sessionSetter, void* self,
                               uintptr_t callback, uintptr_t randomSource) {
    const ByteView session = LocalTlsSessionId(), master = LocalTlsMasterSecret();
    return glide_49284_construct_with_local_session(construct, sessionSetter, self, callback, randomSource,
                                                    session.data, static_cast<unsigned>(session.size), master.data,
                                                    static_cast<unsigned>(master.size));
}
void InitializeHotCipher(void* state) {
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
const std::vector<HotSymbol>& HotTransportSymbols() {
#define SYMBOL(name)                                                                                                   \
    {                                                                                                                  \
        #name, {                                                                                                       \
            #name                                                                                                      \
        }                                                                                                              \
    }
    static const std::vector<HotSymbol> symbols = {
        SYMBOL(client_49284_tls_construct), SYMBOL(client_49284_tls_set_session), SYMBOL(client_49284_msgconn_rc4)};
#undef SYMBOL
    return symbols;
}
const std::vector<HookSpec>& HotTransportHooks() {
    static const std::vector<HookSpec> hooks = {{HotTransportSymbols()[0].name, HotTransportSymbols()[0].symbol,
                                                 reinterpret_cast<void*>(Construct),
                                                 reinterpret_cast<void**>(&originalConstruct), true},
                                                {HotTransportSymbols()[2].name, HotTransportSymbols()[2].symbol,
                                                 reinterpret_cast<void*>(Cipher), &originalCipher, true}};
    return hooks;
}
bool PrepareHotTransport(ClientRuntime& runtime, const PeImage& image, void* module, std::string& error) {
    error = "HoT transport requires an unprepared build 49284 x86 client";
    const BuildInfo* build = nullptr;
    if (prepared || !module || sizeof(void*) != 4 || runtime.config.build != 49284 || image.Machine() != Bitness::X86 ||
        DetectBuild(image, 49284, build) != BuildDetection::Found)
        return false;
    uint32_t setter = 0;
    for (size_t i = 0; i < HotTransportSymbols().size(); ++i) {
        const auto& symbol = HotTransportSymbols()[i];
        const auto match = ResolveSymbol(image, symbol.symbol);
        if (match.count != 1) {
            error = std::string("HoT transport RVA is not verified: ") + symbol.name;
            return false;
        }
        if (i == 1)
            setter = match.rva;
    }
    setSession = reinterpret_cast<Glide49284TlsSetSessionFn>(static_cast<uint8_t*>(module) + setter);
    prepared = true;
    error.clear();
    return true;
}
} // namespace glide
