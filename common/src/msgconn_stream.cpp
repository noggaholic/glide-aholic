#include "glide/msgconn_stream.h"
#include <cstring>

namespace glide {
bool MsgConnStream::RawRead(uint8_t* data, size_t count) {
    const size_t cached = buffered_.Size() < count ? buffered_.Size() : count;
    if (cached) {
        std::memcpy(data, buffered_.Data(), cached);
        buffered_.Consume(cached);
    }
    return ReadExact(stream_, data + cached, count - cached);
}
bool MsgConnStream::ReadInit(ByteView initial, InitHeader& out) {
    if (initialized_ || failed_ || !initial.Valid() || initial.size > 65536)
        return false;
    failed_ = true;
    if (!buffered_.Append(initial))
        return false;
    uint8_t header[16];
    if (!RawRead(header, sizeof(header)))
        return false;
    Reader reader(ByteView(header, sizeof(header)));
    ByteView fields;
    uint32_t extra = 0;
    if (!reader.Bytes(12, fields) || !reader.U32(extra) || (extra != 4 && extra != 72))
        return false;
    uint8_t extension[68];
    const size_t size = extra - 4;
    if (!RawRead(extension, size) || !init_.Append(ByteView(header, sizeof(header))) ||
        !init_.Append(ByteView(extension, size)))
        return false;
    InitHeader result;
    if (DecodeInit(init_.View(), result) != DecodeStatus::Complete)
        return false;
    out = result;
    initialized_ = true;
    failed_ = false;
    return true;
}
bool MsgConnStream::StartCipher(KeyExchangeRecord record) {
    if (!initialized_ || active_ || failed_)
        return false;
    failed_ = true;
    uint8_t keyExchange[66];
    const uint8_t reply[22] = {1, 22};
    const uint8_t key[] = "GLIDE_AHOLIC_RC4_KEY";
    static_assert(sizeof(key) - 1 == 20, "Local RC4 key size");
    if (record == KeyExchangeRecord::LegacyFixed66) {
        if (!RawRead(keyExchange, sizeof(keyExchange)))
            return false;
    } else if (record == KeyExchangeRecord::SizedAligned) {
        // Retained HoT sender: type0, total size2..66, payload aligned to4.
        // Consume exactly this record, retaining any following encrypted data.
        if (!RawRead(keyExchange, 2) || keyExchange[0] || keyExchange[1] < 2 || keyExchange[1] > 66 ||
            ((keyExchange[1] - 2) & 3) || !RawRead(keyExchange + 2, keyExchange[1] - 2))
            return false;
    } else
        return false;
    if (!stream_.Write(ByteView(reply, sizeof(reply))) || !incoming_.Initialize(ByteView(key, sizeof(key) - 1)) ||
        !outgoing_.Initialize(ByteView(key, sizeof(key) - 1)))
        return false;
    active_ = true;
    failed_ = false;
    return true;
}
bool MsgConnStream::Read(uint8_t* data, size_t capacity, size_t& received) {
    received = 0;
    if (!active_ || failed_ || !data || !capacity)
        return false;
    if (buffered_.Size()) {
        received = buffered_.Size() < capacity ? buffered_.Size() : capacity;
        std::memcpy(data, buffered_.Data(), received);
        buffered_.Consume(received);
    } else if (!stream_.Read(data, capacity, received) || !received || received > capacity) {
        failed_ = true;
        return false;
    }
    return incoming_.Apply(data, received);
}
bool MsgConnStream::Send(ByteView payload, bool framed) {
    std::lock_guard<std::mutex> guard(sendMutex_);
    if (!active_ || failed_ || !payload.Valid() || !payload.size || payload.size > UINT16_MAX)
        return false;
    Buffer wire;
    if (!(framed ? EncodeRawFrame(payload, wire) : wire.Append(payload)))
        return false;
    // A failed send has already advanced the stream cipher and cannot be retried.
    if (!outgoing_.Apply(wire.Data(), wire.Size()) || !stream_.Write(wire.View())) {
        failed_ = true;
        return false;
    }
    return true;
}
} // namespace glide
