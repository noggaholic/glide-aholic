#pragma once
#include "glide/buffer.h"

namespace glide {
// RFC 5054 group 1024, generator 2, used by the retained beta reference.
// Inputs are prepared UTF-8 bytes; text normalization belongs to the caller.
ByteView Srp1024Prime();
bool Srp1024Verifier(ByteView identity, ByteView password, ByteView salt, Buffer& out);
class Srp1024Server {
  public:
    Srp1024Server() = default;
    ~Srp1024Server();
    Srp1024Server(const Srp1024Server&) = delete;
    Srp1024Server& operator=(const Srp1024Server&) = delete;
    // Caller supplies fresh 32-byte cryptographic randomness for each handshake.
    bool Begin(ByteView verifier, ByteView privateValue);
    ByteView PublicValue() const {
        return ready_ ? ByteView(public_, sizeof(public_)) : ByteView();
    }
    // Single-use, including failure. Successful output is the unpadded premaster.
    // TLS Finished must still verify it before the caller authorizes an account.
    bool Secret(ByteView clientPublic, Buffer& out);

  private:
    void Clear();
    uint8_t verifier_[128] = {}, private_[32] = {}, public_[128] = {};
    bool ready_ = false;
};
} // namespace glide
