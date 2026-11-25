#ifndef GLIDE_CLIENT_49284_TRANSPORT_H
#define GLIDE_CLIENT_49284_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>

/* x86 native ABI: ECX=self; EDX is not an argument. __fastcall's explicit
 * unused second argument preserves that ABI and the native RET8/RET16.
 * The opaque constructor arguments and return value pass through unchanged.
 * This header is also exercised with C callbacks; it doesn't run a game EXE. */
typedef void*(__fastcall* Glide49284TlsConstructFn)(void* self, void* unused_edx, uintptr_t callback,
                                                    uintptr_t random_source);
typedef void(__fastcall* Glide49284TlsSetSessionFn)(void* self, void* unused_edx, const void* session,
                                                    unsigned session_size, const void* master, unsigned master_size);

static __inline void* glide_49284_construct_with_local_session(Glide49284TlsConstructFn construct,
                                                               Glide49284TlsSetSessionFn set_session, void* self,
                                                               uintptr_t callback, uintptr_t random_source,
                                                               const void* session, unsigned session_size,
                                                               const void* master, unsigned master_size) {
    void* result = construct(self, NULL, callback, random_source);
    /* Both base state0 and the private array exist now. Native SetSession
     * enforces its normal invariants; the original ClientHello/ServerHello,
     * certificate handling, PRF and record codec are not bypassed. */
    set_session(self, NULL, session, session_size, master, master_size);
    return result;
}

#endif
