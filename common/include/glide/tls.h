#pragma once
#include "glide/socket.h"
#include <mutex>
#include <atomic>
#include <string>

namespace glide {
constexpr uint16_t kTlsVersion = 0x0303;
constexpr uint16_t kTlsCipherSuite = 0x003d;
constexpr size_t kTlsMaxPlaintext = 16384;
constexpr size_t kTlsMaxCiphertext = kTlsMaxPlaintext + 2048;
ByteView LocalTlsSessionId();
ByteView LocalTlsMasterSecret();
enum class TlsVariant { ResumedSession, June2012 };
using TlsEntropy = bool (*)(uint8_t*, size_t);
class TlsSrpAccounts {
  public:
    virtual ~TlsSrpAccounts() {}
    // Lookup only; this call must not authorize a session. Identity is unmodified.
    virtual bool Lookup(const std::string& identity, Buffer& salt, Buffer& verifier) = 0;
};

class TlsRecordCipher {
  public:
    TlsRecordCipher() = default;
    ~TlsRecordCipher();
    TlsRecordCipher(const TlsRecordCipher&) = delete;
    TlsRecordCipher& operator=(const TlsRecordCipher&) = delete;
    void Initialize(const uint8_t* macKey, const uint8_t encryptionKey[32],
                    TlsVariant variant = TlsVariant::ResumedSession);
    bool Encrypt(uint8_t type, ByteView plain, Buffer& out);
    bool Decrypt(uint8_t type, ByteView encrypted, Buffer& out);

  private:
    bool Mac(uint8_t type, ByteView plain, uint8_t digest[32]) const;
    uint8_t macKey_[32] = {}, key_[32] = {};
    uint64_t sequence_ = 0;
    size_t macSize_ = 32;
    bool ready_ = false;
};

class TlsSession : public ByteStream {
  public:
    explicit TlsSession(ByteStream& stream, TlsVariant variant = TlsVariant::ResumedSession,
                        TlsEntropy entropy = nullptr)
        : stream_(stream), variant_(variant), entropy_(entropy) {}
    bool Accept(ByteView initial, std::string& error);
    bool AcceptSrp(ByteView initial, TlsSrpAccounts& accounts, std::string& error);
    bool Active() const {
        return active_;
    }
    bool PasswordVerified() const {
        return active_ && passwordVerified_;
    }
    // Resumed connections expose identity without password proof; check PasswordVerified.
    const std::string& LoginName() const {
        return loginName_;
    }
    bool Read(uint8_t* data, size_t capacity, size_t& received) override;
    bool Write(ByteView data) override;

  private:
    bool RawRead(uint8_t* data, size_t count);
    bool ReceiveRecord(uint8_t& type, uint16_t& version, Buffer& data);
    bool SendRecord(uint8_t type, ByteView data);
    bool BeginHandshake(ByteView initial, Buffer& hello, std::string& error);
    bool ReadHandshake(Buffer& out, size_t maximum, bool legacyRecordVersion);
    bool Random(uint8_t* out, size_t size);
    ByteStream& stream_;
    TlsVariant variant_;
    TlsEntropy entropy_;
    std::string loginName_;
    Buffer raw_, plain_;
    TlsRecordCipher incoming_, outgoing_;
    std::mutex writeMutex_;
    std::atomic<bool> active_{false};
    bool started_ = false;
    bool passwordVerified_ = false;
};
} // namespace glide
