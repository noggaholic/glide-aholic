#include "glide/crypto.h"
#include <Windows.h>
#include <bcrypt.h>
#include <climits>
#include <cstring>

namespace glide {
namespace {
struct Algorithm {
    BCRYPT_ALG_HANDLE handle = nullptr;
    ~Algorithm() {
        if (handle)
            BCryptCloseAlgorithmProvider(handle, 0);
    }
};
struct Hash {
    BCRYPT_HASH_HANDLE handle = nullptr;
    ~Hash() {
        if (handle)
            BCryptDestroyHash(handle);
    }
};
struct Key {
    BCRYPT_KEY_HANDLE handle = nullptr;
    ~Key() {
        if (handle)
            BCryptDestroyKey(handle);
    }
};
bool HashBytes(bool hmac, ByteView key, ByteView input, uint8_t* digest, bool sha1 = false) {
    if (!key.Valid() || !input.Valid() || !digest || key.size > ULONG_MAX || input.size > ULONG_MAX)
        return false;
    Algorithm algorithm;
    Buffer storage;
    Hash hash;
    DWORD objectSize = 0, resultSize = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm.handle, sha1 ? BCRYPT_SHA1_ALGORITHM : BCRYPT_SHA256_ALGORITHM, nullptr,
                                    hmac ? BCRYPT_ALG_HANDLE_HMAC_FLAG : 0) < 0 ||
        BCryptGetProperty(algorithm.handle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &resultSize, 0) < 0 ||
        !storage.Reserve(objectSize) ||
        BCryptCreateHash(algorithm.handle, &hash.handle, storage.Data(), objectSize, const_cast<PUCHAR>(key.data),
                         static_cast<ULONG>(key.size), 0) < 0 ||
        BCryptHashData(hash.handle, const_cast<PUCHAR>(input.data), static_cast<ULONG>(input.size), 0) < 0)
        return false;
    uint8_t result[32];
    const ULONG digestSize = sha1 ? 20 : 32;
    if (BCryptFinishHash(hash.handle, result, digestSize, 0) < 0)
        return false;
    std::memcpy(digest, result, digestSize);
    SecureZeroMemory(result, sizeof(result));
    return true;
}
} // namespace
bool RandomBytes(uint8_t* output, size_t size) {
    if ((!output && size) || size > ULONG_MAX)
        return false;
    return size == 0 ||
           BCryptGenRandom(nullptr, output, static_cast<ULONG>(size), BCRYPT_USE_SYSTEM_PREFERRED_RNG) >= 0;
}
bool Sha256(ByteView input, uint8_t digest[32]) {
    return HashBytes(false, ByteView(), input, digest);
}
bool Sha1(ByteView input, uint8_t digest[20]) {
    return HashBytes(false, ByteView(), input, digest, true);
}
bool HmacSha256(ByteView key, ByteView input, uint8_t digest[32]) {
    return HashBytes(true, key, input, digest);
}
bool HmacSha1(ByteView key, ByteView input, uint8_t digest[20]) {
    return HashBytes(true, key, input, digest, true);
}
bool Aes256Cbc(bool encrypt, const uint8_t key[32], const uint8_t iv[16], ByteView input, uint8_t* output) {
    if (!key || !iv || !input.Valid() || (!output && input.size) || input.size > ULONG_MAX || (input.size & 15) != 0)
        return false;
    if (!input.size)
        return true;
    Algorithm algorithm;
    Buffer storage;
    Key aes;
    DWORD objectSize = 0, resultSize = 0;
    if (BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) < 0 ||
        BCryptSetProperty(algorithm.handle, BCRYPT_CHAINING_MODE,
                          reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                          sizeof(BCRYPT_CHAIN_MODE_CBC), 0) < 0 ||
        BCryptGetProperty(algorithm.handle, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectSize),
                          sizeof(objectSize), &resultSize, 0) < 0 ||
        !storage.Reserve(objectSize) ||
        BCryptGenerateSymmetricKey(algorithm.handle, &aes.handle, storage.Data(), objectSize, const_cast<PUCHAR>(key),
                                   32, 0) < 0)
        return false;
    uint8_t localIv[16];
    std::memcpy(localIv, iv, sizeof(localIv));
    ULONG written = 0;
    const ULONG size = static_cast<ULONG>(input.size);
    const NTSTATUS result = encrypt ? BCryptEncrypt(aes.handle, const_cast<PUCHAR>(input.data), size, nullptr, localIv,
                                                    sizeof(localIv), output, size, &written, 0)
                                    : BCryptDecrypt(aes.handle, const_cast<PUCHAR>(input.data), size, nullptr, localIv,
                                                    sizeof(localIv), output, size, &written, 0);
    SecureZeroMemory(localIv, sizeof(localIv));
    return result >= 0 && written == size;
}
bool TlsPrf(ByteView secret, const char* label, ByteView seed, uint8_t* output, size_t size) {
    if (!secret.Valid() || !seed.Valid() || !label || (!output && size))
        return false;
    Buffer fullSeed, input;
    if (!fullSeed.Append(ByteView(label, std::strlen(label))) || !fullSeed.Append(seed))
        return false;
    struct Scratch {
        uint8_t a[32] = {}, block[32] = {};
        ~Scratch() {
            SecureZeroMemory(a, sizeof(a));
            SecureZeroMemory(block, sizeof(block));
        }
    } scratch;
    if (!HmacSha256(secret, fullSeed.View(), scratch.a))
        return false;
    size_t written = 0;
    while (written < size) {
        input.Clear();
        if (!input.Append(ByteView(scratch.a, sizeof(scratch.a))) || !input.Append(fullSeed.View()) ||
            !HmacSha256(secret, input.View(), scratch.block))
            return false;
        const size_t take = size - written < sizeof(scratch.block) ? size - written : sizeof(scratch.block);
        std::memcpy(output + written, scratch.block, take);
        written += take;
        if (!HmacSha256(secret, ByteView(scratch.a, sizeof(scratch.a)), scratch.a))
            return false;
    }
    return true;
}
} // namespace glide
