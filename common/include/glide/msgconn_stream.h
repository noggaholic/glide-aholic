#pragma once
#include "glide/msgconn.h"
#include "glide/socket.h"
#include "glide/rc4.h"
#include <mutex>

namespace glide {
class MsgConnStream {
  public:
    explicit MsgConnStream(ByteStream& stream) : stream_(stream) {}
    bool ReadInit(ByteView initial, InitHeader& out);
    bool StartCipher(KeyExchangeRecord record = KeyExchangeRecord::LegacyFixed66);
    bool Read(uint8_t* data, size_t capacity, size_t& received);
    bool Send(ByteView payload, bool framed);
    bool HasBufferedInput() const {
        return buffered_.Size() != 0;
    }

  private:
    bool RawRead(uint8_t* data, size_t count);
    ByteStream& stream_;
    Buffer buffered_, init_;
    Rc4 incoming_, outgoing_;
    std::mutex sendMutex_;
    bool initialized_ = false, active_ = false;
    std::atomic<bool> failed_{false};
};
} // namespace glide
