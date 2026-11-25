#include "glide/srp.h"
#include "glide/crypto.h"
#include "srp_math.h"
#include <Windows.h>
#include <cstring>
#include <utility>

namespace glide {
namespace {
// Same group as the retained reGW2 reference; RFC 5054 Appendix A (1024 bits).
const char primeHex[] = "EEAF0AB9ADB38DD69C33F80AFA8FC5E86072618775FF3C0B9EA2314C9C256576D6"
                        "74DF7496EA81D3383B4813D692C6E0E0D5D8E250B98BE48E495C1D6089DAD15DC7"
                        "D7B46154D6B6CE8EF4AD69B15D4982559B297BCF1885C529F566660E57EC68EDBC3"
                        "C05726CC02FD4CBF4976EAA9AFD5138FE8376435B9FC61D2FC0EB06E3";
using namespace srp_detail;

} // namespace
ByteView Srp1024Prime() {
    struct Bytes {
        uint8_t data[128] = {};
        Bytes() {
            static_assert(sizeof(primeHex) == 257, "1024-bit prime");
            for (size_t i = 0; i < 256; ++i) {
                const unsigned value = primeHex[i] <= '9' ? primeHex[i] - '0' : primeHex[i] - 'A' + 10;
                data[i / 2] |= static_cast<uint8_t>(value << ((i & 1) ? 0 : 4));
            }
        }
    };
    static const Bytes bytes;
    return ByteView(bytes.data, sizeof(bytes.data));
}
bool Srp1024Verifier(ByteView identity, ByteView password, ByteView salt, Buffer& out) {
    if (!identity.Valid() || !password.Valid() || !salt.Valid() || !identity.size || identity.size > 255 ||
        !password.size || password.size > 4096 || !salt.size || salt.size > 255)
        return false;
    Scratch input, salted;
    Digest inner, exponent;
    if (!input.bytes.Reserve(identity.size + 1 + password.size) || !input.bytes.Append(identity) ||
        !input.bytes.U8(':') || !input.bytes.Append(password) || !Sha1(input.bytes.View(), inner.bytes) ||
        !salted.bytes.Append(salt) || !salted.bytes.Append(ByteView(inner.bytes, 20)) ||
        !Sha1(salted.bytes.View(), exponent.bytes))
        return false;
    Number generator;
    generator.word[0] = 2;
    return AppendNumber(Power(generator, ByteView(exponent.bytes, 20)), out, false);
}
void Srp1024Server::Clear() {
    ready_ = false;
    SecureZeroMemory(verifier_, sizeof(verifier_));
    SecureZeroMemory(private_, sizeof(private_));
    SecureZeroMemory(public_, sizeof(public_));
}
Srp1024Server::~Srp1024Server() {
    Clear();
}
bool Srp1024Server::Begin(ByteView verifier, ByteView privateValue) {
    Clear();
    Number verified;
    if (!Value(verifier, verified) || !privateValue.Valid() || privateValue.size != 32 || Zero(Read(privateValue)))
        return false;
    Number generator;
    generator.word[0] = 2;
    uint8_t paddedGenerator[128] = {};
    paddedGenerator[127] = 2;
    Buffer input;
    Digest multiplier;
    if (!input.Append(Srp1024Prime()) || !input.Append(ByteView(paddedGenerator, 128)) ||
        !Sha1(input.View(), multiplier.bytes))
        return false;
    const Number value = Add(Product(Read(ByteView(multiplier.bytes, 20)), verified), Power(generator, privateValue));
    if (Zero(value))
        return false;
    Write(verified, verifier_);
    Write(value, public_);
    std::memcpy(private_, privateValue.data, 32);
    ready_ = true;
    return true;
}
bool Srp1024Server::Secret(ByteView clientPublic, Buffer& out) {
    struct Cleanup {
        Srp1024Server& server;
        ~Cleanup() {
            server.Clear();
        }
    } cleanup{*this};
    Number client;
    if (!ready_ || !Value(clientPublic, client))
        return false;
    uint8_t paddedClient[128];
    Write(client, paddedClient);
    Buffer input;
    Digest scramble;
    if (!input.Append(ByteView(paddedClient, 128)) || !input.Append(ByteView(public_, 128)) ||
        !Sha1(input.View(), scramble.bytes) || Zero(Read(ByteView(scramble.bytes, 20))))
        return false;
    const Number verifier = Read(ByteView(verifier_, 128));
    const Number mixed = Product(client, Power(verifier, ByteView(scramble.bytes, 20)));
    return AppendNumber(Power(mixed, ByteView(private_, 32)), out, true);
}
} // namespace glide
