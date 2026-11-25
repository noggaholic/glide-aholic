#pragma once
#include "startup.h"
#include "native_transport.h"
namespace glide {
const std::vector<HotSymbol>& HotTransportSymbols();
const std::vector<HookSpec>& HotTransportHooks();
bool PrepareHotTransport(ClientRuntime&, const PeImage&, void* module, std::string& error);
// Native MsgConn state has two uint32 cursors followed by 256 permutation bytes.
void InitializeHotCipher(void* state);
void* ConstructHotLocalSession(Glide49284TlsConstructFn, Glide49284TlsSetSessionFn, void* self, uintptr_t callback,
                               uintptr_t randomSource);
} // namespace glide
