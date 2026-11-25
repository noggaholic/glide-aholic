#include "glide/tls.h"
#include "glide/crypto.h"
#include "glide/srp.h"
#include <Windows.h>
#include <cstring>
#include <vector>

namespace glide {
namespace {
const uint8_t sessionId[] = "GLIDEAHOLIC_SESSION_RESUME_ID_01";
const uint8_t masterSecret[] = "GLIDEAHOLIC_MASTER_SECRET_0123456789ABCDEF012345";
static_assert(sizeof(sessionId) - 1 == 32, "Session ID size");
static_assert(sizeof(masterSecret) - 1 == 48, "Master secret size");
uint16_t Be16(const uint8_t* p) {
    return static_cast<uint16_t>((p[0] << 8) | p[1]);
}
void Put16(uint8_t* p, uint16_t value) {
    p[0] = static_cast<uint8_t>(value >> 8);
    p[1] = static_cast<uint8_t>(value);
}
bool Equal(const uint8_t* a, const uint8_t* b, size_t size) {
    uint8_t different = 0;
    for (size_t i = 0; i < size; ++i)
        different |= a[i] ^ b[i];
    return different == 0;
}
bool Finished(const char* label, ByteView transcript, uint8_t output[12], ByteView master = LocalTlsMasterSecret()) {
    uint8_t digest[32];
    const bool ok = Sha256(transcript, digest) && TlsPrf(master, label, ByteView(digest, sizeof(digest)), output, 12);
    SecureZeroMemory(digest, sizeof(digest));
    return ok;
}
bool ClientHello(ByteView hello, uint8_t random[32], TlsVariant variant, std::string& login) {
    if (hello.size < 41 || hello.data[0] != 1 || Be16(hello.data + 4) != kTlsVersion)
        return false;
    std::memcpy(random, hello.data + 6, 32);
    size_t at = 38;
    const size_t sessionSize = hello.data[at++];
    if (sessionSize > 32 || sessionSize + 2 > hello.size - at)
        return false;
    // The injected ServerHello handler supplies the shared session even on first login.
    at += sessionSize;
    const size_t suitesSize = Be16(hello.data + at);
    at += 2;
    if (suitesSize == 0 || (suitesSize & 1) || suitesSize >= hello.size - at)
        return false;
    bool cipherFound = false;
    for (size_t end = at + suitesSize; at < end; at += 2)
        if (Be16(hello.data + at) == (variant == TlsVariant::June2012 ? 0xc020 : kTlsCipherSuite))
            cipherFound = true;
    if (!cipherFound)
        return false;
    const size_t compressionSize = hello.data[at++];
    if (compressionSize == 0 || compressionSize > hello.size - at)
        return false;
    bool nullCompression = false;
    for (size_t end = at + compressionSize; at < end; ++at)
        if (hello.data[at] == 0)
            nullCompression = true;
    if (!nullCompression)
        return false;
    if (at == hello.size)
        return variant != TlsVariant::June2012;
    if (hello.size - at < 2)
        return false;
    const size_t extensionSize = Be16(hello.data + at);
    at += 2;
    if (extensionSize != hello.size - at)
        return false;
    while (at < hello.size) {
        if (hello.size - at < 4)
            return false;
        const uint16_t extension = Be16(hello.data + at);
        const size_t size = Be16(hello.data + at + 2);
        at += 4;
        if (size > hello.size - at)
            return false;
        if (variant == TlsVariant::June2012 && extension == 12) {
            if (!login.empty() || size < 2 || size > 64 || hello.data[at] != size - 1)
                return false;
            for (size_t i = 1; i < size; ++i)
                if (hello.data[at + i] < 0x21 || hello.data[at + i] > 0x7e)
                    return false;
            login.assign(reinterpret_cast<const char*>(hello.data + at + 1), size - 1);
        }
        at += size;
    }
    return variant != TlsVariant::June2012 || !login.empty();
}
} // namespace
ByteView LocalTlsSessionId() {
    return ByteView(sessionId, sizeof(sessionId) - 1);
}
ByteView LocalTlsMasterSecret() {
    return ByteView(masterSecret, sizeof(masterSecret) - 1);
}
TlsRecordCipher::~TlsRecordCipher() {
    SecureZeroMemory(macKey_, sizeof(macKey_));
    SecureZeroMemory(key_, sizeof(key_));
}
void TlsRecordCipher::Initialize(const uint8_t* macKey, const uint8_t encryptionKey[32], TlsVariant variant) {
    ready_ = false;
    sequence_ = 0;
    SecureZeroMemory(macKey_, sizeof(macKey_));
    SecureZeroMemory(key_, sizeof(key_));
    if (variant != TlsVariant::ResumedSession && variant != TlsVariant::June2012)
        return;
    macSize_ = variant == TlsVariant::June2012 ? 20 : 32;
    if (!macKey || !encryptionKey)
        return;
    std::memcpy(macKey_, macKey, macSize_);
    std::memcpy(key_, encryptionKey, sizeof(key_));
    ready_ = true;
}
bool TlsRecordCipher::Mac(uint8_t type, ByteView plain, uint8_t digest[32]) const {
    uint8_t header[13];
    for (unsigned i = 0; i < 8; ++i)
        header[7 - i] = static_cast<uint8_t>(sequence_ >> (i * 8));
    header[8] = type;
    Put16(header + 9, kTlsVersion);
    Put16(header + 11, static_cast<uint16_t>(plain.size));
    Buffer input;
    return input.Append(ByteView(header, sizeof(header))) && input.Append(plain) &&
           (macSize_ == 20 ? HmacSha1(ByteView(macKey_, macSize_), input.View(), digest)
                           : HmacSha256(ByteView(macKey_, macSize_), input.View(), digest));
}
bool TlsRecordCipher::Encrypt(uint8_t type, ByteView plain, Buffer& out) {
    if (!ready_ || sequence_ == UINT64_MAX || !plain.Valid() || plain.size > kTlsMaxPlaintext)
        return false;
    uint8_t mac[32];
    if (!Mac(type, plain, mac))
        return false;
    Buffer block;
    if (!block.Append(plain) || !block.Append(ByteView(mac, macSize_)))
        return false;
    SecureZeroMemory(mac, sizeof(mac));
    const uint8_t padding = static_cast<uint8_t>(15 - (block.Size() & 15));
    for (size_t i = 0; i <= padding; ++i)
        if (!block.U8(padding))
            return false;
    std::vector<uint8_t> encrypted(block.Size() + 16);
    if (!RandomBytes(encrypted.data(), 16) ||
        !Aes256Cbc(true, key_, encrypted.data(), block.View(), encrypted.data() + 16))
        return false;
    Buffer result;
    if (!result.Append(ByteView(encrypted.data(), encrypted.size())))
        return false;
    ++sequence_;
    out = std::move(result);
    return true;
}
bool TlsRecordCipher::Decrypt(uint8_t type, ByteView encrypted, Buffer& out) {
    if (!ready_)
        return false;
    ready_ = false; // A failed record invalidates this direction; sequence reuse is forbidden.
    if (sequence_ == UINT64_MAX || !encrypted.Valid() || encrypted.size < (macSize_ == 20 ? 48u : 64u) ||
        encrypted.size > kTlsMaxCiphertext || ((encrypted.size - 16) & 15))
        return false;
    std::vector<uint8_t> block(encrypted.size - 16);
    if (!Aes256Cbc(false, key_, encrypted.data, ByteView(encrypted.data + 16, block.size()), block.data()))
        return false;
    const size_t padding = static_cast<size_t>(block.back()) + 1;
    if (padding > block.size() || block.size() - padding < macSize_)
        return false;
    uint8_t different = 0;
    for (size_t i = block.size() - padding; i < block.size(); ++i)
        different |= block[i] ^ block.back();
    const size_t size = block.size() - padding - macSize_;
    if (different || size > kTlsMaxPlaintext)
        return false;
    uint8_t expected[32];
    if (!Mac(type, ByteView(block.data(), size), expected))
        return false;
    const bool valid = Equal(expected, block.data() + size, macSize_);
    SecureZeroMemory(expected, sizeof(expected));
    if (!valid)
        return false;
    Buffer result;
    if (!result.Append(ByteView(block.data(), size)))
        return false;
    ++sequence_;
    ready_ = true;
    out = std::move(result);
    return true;
}
bool TlsSession::RawRead(uint8_t* data, size_t count) {
    const size_t cached = raw_.Size() < count ? raw_.Size() : count;
    if (cached) {
        std::memcpy(data, raw_.Data(), cached);
        raw_.Consume(cached);
    }
    return ReadExact(stream_, data + cached, count - cached);
}
bool TlsSession::ReceiveRecord(uint8_t& type, uint16_t& version, Buffer& data) {
    uint8_t header[5];
    if (!RawRead(header, sizeof(header)))
        return false;
    const size_t size = Be16(header + 3);
    if (size > kTlsMaxCiphertext)
        return false;
    std::vector<uint8_t> payload(size);
    if (size && !RawRead(payload.data(), size))
        return false;
    Buffer result;
    if (!result.Append(ByteView(payload.data(), size)))
        return false;
    type = header[0];
    version = Be16(header + 1);
    data = std::move(result);
    return true;
}
bool TlsSession::SendRecord(uint8_t type, ByteView data) {
    if (!data.Valid() || data.size > kTlsMaxCiphertext)
        return false;
    uint8_t header[5];
    header[0] = type;
    Put16(header + 1, kTlsVersion);
    Put16(header + 3, static_cast<uint16_t>(data.size));
    return stream_.Write(ByteView(header, sizeof(header))) && stream_.Write(data);
}
bool TlsSession::Random(uint8_t* out, size_t size) {
    return entropy_ ? entropy_(out, size) : RandomBytes(out, size);
}
bool TlsSession::ReadHandshake(Buffer& out, size_t maximum, bool legacyRecordVersion) {
    Buffer message, record;
    size_t expected = 0;
    do {
        uint8_t type = 0;
        uint16_t version = 0;
        if (!ReceiveRecord(type, version, record) || type != 0x16 ||
            (version != kTlsVersion && !(legacyRecordVersion && version == 0x0301)) || !record.Size() ||
            record.Size() > kTlsMaxPlaintext || record.Size() > maximum - message.Size() ||
            !message.Append(record.View()))
            return false;
        if (message.Size() >= 4) {
            expected = 4 + (static_cast<size_t>(message.Data()[1]) << 16) +
                       (static_cast<size_t>(message.Data()[2]) << 8) + message.Data()[3];
            if (expected > maximum || message.Size() > expected)
                return false;
        }
    } while (!expected || message.Size() < expected);
    out = std::move(message);
    return true;
}
bool TlsSession::BeginHandshake(ByteView initial, Buffer& hello, std::string& error) {
    if (started_) {
        error = "TLS already started";
        return false;
    }
    started_ = true;
    if (variant_ != TlsVariant::ResumedSession && variant_ != TlsVariant::June2012) {
        error = "Unsupported TLS variant";
        return false;
    }
    if (!initial.Valid() || initial.size > kTlsMaxCiphertext + 5 || !raw_.Append(initial)) {
        error = "Invalid buffered TLS data";
        return false;
    }
    if (!ReadHandshake(hello, 65536, true)) {
        error = "Invalid ClientHello records";
        return false;
    }
    return true;
}
bool TlsSession::Accept(ByteView initial, std::string& error) {
    Buffer hello, record;
    if (!BeginHandshake(initial, hello, error))
        return false;
    uint8_t type = 0;
    uint16_t version = 0;
    uint8_t clientRandom[32], serverRandom[32];
    std::string login;
    if (!ClientHello(hello.View(), clientRandom, variant_, login) || !Random(serverRandom, sizeof(serverRandom))) {
        error = "Unsupported ClientHello";
        return false;
    }
    uint8_t serverHello[74] = {2, 0, 0, 70};
    Put16(serverHello + 4, kTlsVersion);
    std::memcpy(serverHello + 6, serverRandom, 32);
    serverHello[38] = 32;
    std::memcpy(serverHello + 39, sessionId, 32);
    Put16(serverHello + 71, variant_ == TlsVariant::June2012 ? 0xc020 : kTlsCipherSuite);
    Buffer transcript;
    if (!transcript.Append(hello.View()) || !transcript.Append(ByteView(serverHello, sizeof(serverHello)))) {
        error = "Cannot store TLS transcript";
        return false;
    }
    uint8_t seed[64], keys[128];
    const size_t macSize = variant_ == TlsVariant::June2012 ? 20 : 32;
    std::memcpy(seed, serverRandom, 32);
    std::memcpy(seed + 32, clientRandom, 32);
    if (!TlsPrf(LocalTlsMasterSecret(), "key expansion", ByteView(seed, sizeof(seed)), keys, 2 * macSize + 64)) {
        error = "Cannot derive TLS keys";
        return false;
    }
    incoming_.Initialize(keys, keys + 2 * macSize, variant_);
    outgoing_.Initialize(keys + macSize, keys + 2 * macSize + 32, variant_);
    SecureZeroMemory(keys, sizeof(keys));
    const uint8_t ccs = 1;
    uint8_t finished[16] = {0x14, 0, 0, 12};
    Buffer encrypted;
    if (!SendRecord(0x16, ByteView(serverHello, sizeof(serverHello))) || !SendRecord(0x14, ByteView(&ccs, 1)) ||
        !Finished("server finished", transcript.View(), finished + 4) ||
        !outgoing_.Encrypt(0x16, ByteView(finished, sizeof(finished)), encrypted) ||
        !SendRecord(0x16, encrypted.View()) || !transcript.Append(ByteView(finished, sizeof(finished)))) {
        error = "Cannot send TLS server completion";
        return false;
    }
    if (!ReceiveRecord(type, version, record) || type != 0x14 || version != kTlsVersion || record.Size() != 1 ||
        record.Data()[0] != 1) {
        error = "Invalid client ChangeCipherSpec";
        return false;
    }
    Buffer clientFinished;
    do {
        Buffer part;
        if (!ReceiveRecord(type, version, record) || type != 0x16 || version != kTlsVersion ||
            !incoming_.Decrypt(type, record.View(), part) || !part.Size() || part.Size() > 16 - clientFinished.Size() ||
            !clientFinished.Append(part.View())) {
            error = "Invalid client Finished record";
            return false;
        }
    } while (clientFinished.Size() < 16);
    if (std::memcmp(clientFinished.Data(), "\x14\0\0\x0c", 4) != 0 ||
        !Finished("client finished", transcript.View(), finished + 4) ||
        !Equal(finished + 4, clientFinished.Data() + 4, 12)) {
        error = "Client Finished transcript mismatch";
        return false;
    }
    loginName_ = std::move(login);
    active_ = true;
    error.clear();
    return true;
}
bool TlsSession::AcceptSrp(ByteView initial, TlsSrpAccounts& accounts, std::string& error) {
    if (variant_ != TlsVariant::June2012) {
        error = "SRP requires the June TLS variant";
        return false;
    }
    Buffer hello;
    if (!BeginHandshake(initial, hello, error))
        return false;
    auto fail = [&](uint8_t alert, const char* reason) {
        error = reason;
        const uint8_t payload[2] = {2, alert};
        SendRecord(0x15, ByteView(payload, sizeof(payload)));
        incoming_.Initialize(nullptr, nullptr);
        outgoing_.Initialize(nullptr, nullptr);
        return false;
    };
    uint8_t clientRandom[32], serverRandom[32];
    std::string login;
    if (!ClientHello(hello.View(), clientRandom, variant_, login))
        return fail(115, "Unsupported SRP ClientHello");
    struct Secrets {
        Buffer salt, verifier, premaster;
        uint8_t privateValue[32] = {}, master[48] = {}, keys[104] = {};
        ~Secrets() {
            for (Buffer* buffer : {&salt, &verifier, &premaster})
                if (buffer->Size())
                    SecureZeroMemory(buffer->Data(), buffer->Size());
            SecureZeroMemory(privateValue, sizeof(privateValue));
            SecureZeroMemory(master, sizeof(master));
            SecureZeroMemory(keys, sizeof(keys));
        }
    } secrets;
    if (!accounts.Lookup(login, secrets.salt, secrets.verifier))
        return fail(115, "SRP account unavailable");
    Srp1024Server exchange;
    if (!secrets.salt.Size() || secrets.salt.Size() > 255 || !Random(serverRandom, sizeof(serverRandom)) ||
        !Random(secrets.privateValue, sizeof(secrets.privateValue)) ||
        !exchange.Begin(secrets.verifier.View(), ByteView(secrets.privateValue, sizeof(secrets.privateValue))))
        return fail(80, "Cannot prepare SRP exchange");
    // Full handshake: no resumable session, client proof precedes server proof.
    uint8_t serverHello[42] = {2, 0, 0, 38};
    Put16(serverHello + 4, kTlsVersion);
    std::memcpy(serverHello + 6, serverRandom, 32);
    Put16(serverHello + 39, 0xc020);
    Buffer parameters;
    ByteView publicValue = exchange.PublicValue();
    while (publicValue.size > 1 && publicValue.data[0] == 0) {
        ++publicValue.data;
        --publicValue.size;
    }
    const uint8_t head[4] = {12, 0, 0, 0};
    auto integer16 = [&](size_t value) {
        uint8_t bytes[2];
        Put16(bytes, static_cast<uint16_t>(value));
        return parameters.Append(ByteView(bytes, 2));
    };
    if (!parameters.Append(ByteView(head, 4)) || !integer16(128) || !parameters.Append(Srp1024Prime()) ||
        !integer16(1) || !parameters.U8(2) || !parameters.U8(static_cast<uint8_t>(secrets.salt.Size())) ||
        !parameters.Append(secrets.salt.View()) || !integer16(publicValue.size) || !parameters.Append(publicValue))
        return fail(80, "Cannot encode SRP parameters");
    Put16(parameters.Data() + 2, static_cast<uint16_t>(parameters.Size() - 4));
    const uint8_t done[4] = {14, 0, 0, 0};
    Buffer transcript;
    if (!transcript.Append(hello.View()) || !transcript.Append(ByteView(serverHello, sizeof(serverHello))) ||
        !transcript.Append(parameters.View()) || !transcript.Append(ByteView(done, 4)) ||
        !SendRecord(0x16, ByteView(serverHello, sizeof(serverHello))) || !SendRecord(0x16, parameters.View()) ||
        !SendRecord(0x16, ByteView(done, 4)))
        return fail(80, "Cannot send SRP parameters");
    Buffer keyExchange;
    if (!ReadHandshake(keyExchange, 134, false) || keyExchange.Size() < 7 || keyExchange.Data()[0] != 16 ||
        static_cast<size_t>(Be16(keyExchange.Data() + 4)) != keyExchange.Size() - 6)
        return fail(50, "Invalid SRP client key exchange");
    if (!exchange.Secret(ByteView(keyExchange.Data() + 6, keyExchange.Size() - 6), secrets.premaster))
        return fail(47, "Invalid SRP public value");
    uint8_t seed[64];
    std::memcpy(seed, clientRandom, 32);
    std::memcpy(seed + 32, serverRandom, 32);
    if (!TlsPrf(secrets.premaster.View(), "master secret", ByteView(seed, sizeof(seed)), secrets.master,
                sizeof(secrets.master)))
        return fail(80, "Cannot derive SRP master secret");
    std::memcpy(seed, serverRandom, 32);
    std::memcpy(seed + 32, clientRandom, 32);
    if (!TlsPrf(ByteView(secrets.master, 48), "key expansion", ByteView(seed, sizeof(seed)), secrets.keys,
                sizeof(secrets.keys)) ||
        !transcript.Append(keyExchange.View()))
        return fail(80, "Cannot derive SRP record keys");
    incoming_.Initialize(secrets.keys, secrets.keys + 40, variant_);
    outgoing_.Initialize(secrets.keys + 20, secrets.keys + 72, variant_);
    Buffer record;
    uint8_t type = 0;
    uint16_t version = 0;
    if (!ReceiveRecord(type, version, record) || type != 0x14 || version != kTlsVersion || record.Size() != 1 ||
        record.Data()[0] != 1)
        return fail(10, "Invalid SRP ChangeCipherSpec");
    Buffer proof;
    do {
        Buffer part;
        if (!ReceiveRecord(type, version, record) || type != 0x16 || version != kTlsVersion)
            return fail(10, "Missing SRP client Finished");
        if (!incoming_.Decrypt(type, record.View(), part))
            return fail(20, "SRP client proof rejected");
        if (!part.Size() || part.Size() > 16 - proof.Size() || !proof.Append(part.View()))
            return fail(50, "Invalid SRP Finished size");
    } while (proof.Size() < 16);
    uint8_t expected[16] = {20, 0, 0, 12};
    if (!Finished("client finished", transcript.View(), expected + 4, ByteView(secrets.master, 48)) ||
        !Equal(expected, proof.Data(), 16))
        return fail(51, "SRP Finished transcript mismatch");
    Buffer encrypted;
    const uint8_t ccs = 1;
    if (!transcript.Append(proof.View()) ||
        !Finished("server finished", transcript.View(), expected + 4, ByteView(secrets.master, 48)) ||
        !outgoing_.Encrypt(0x16, ByteView(expected, 16), encrypted))
        return fail(80, "Cannot prepare SRP server Finished");
    if (!SendRecord(0x14, ByteView(&ccs, 1)) || !SendRecord(0x16, encrypted.View())) {
        incoming_.Initialize(nullptr, nullptr);
        outgoing_.Initialize(nullptr, nullptr);
        error = "Cannot send SRP server Finished";
        return false;
    }
    loginName_ = std::move(login);
    passwordVerified_ = true;
    active_ = true;
    error.clear();
    return true;
}
bool TlsSession::Read(uint8_t* data, size_t capacity, size_t& received) {
    received = 0;
    if (!active_ || !data || !capacity)
        return false;
    while (!plain_.Size()) {
        Buffer record, plain;
        uint8_t type;
        uint16_t version;
        if (!ReceiveRecord(type, version, record) || version != kTlsVersion || (type != 0x17 && type != 0x15) ||
            !incoming_.Decrypt(type, record.View(), plain)) {
            active_ = false;
            return false;
        }
        if (type == 0x15) {
            active_ = false;
            return false;
        }
        plain_ = std::move(plain);
    }
    const size_t take = plain_.Size() < capacity ? plain_.Size() : capacity;
    std::memcpy(data, plain_.Data(), take);
    plain_.Consume(take);
    received = take;
    return true;
}
bool TlsSession::Write(ByteView data) {
    std::lock_guard<std::mutex> guard(writeMutex_);
    if (!active_ || !data.Valid())
        return false;
    size_t at = 0;
    while (at < data.size) {
        const size_t size = data.size - at < kTlsMaxPlaintext ? data.size - at : kTlsMaxPlaintext;
        Buffer encrypted;
        if (!outgoing_.Encrypt(0x17, ByteView(data.data + at, size), encrypted) ||
            !SendRecord(0x17, encrypted.View())) {
            active_ = false;
            return false;
        }
        at += size;
    }
    return true;
}
} // namespace glide
