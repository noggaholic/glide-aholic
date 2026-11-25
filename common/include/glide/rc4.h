#pragma once
#include "glide/buffer.h"

namespace glide {

class Rc4 {
  public:
    Rc4();
    ~Rc4();
    Rc4(const Rc4&) = delete;
    Rc4& operator=(const Rc4&) = delete;
    bool Initialize(ByteView key);
    bool Apply(uint8_t* bytes, size_t size);

  private:
    uint8_t state_[256];
    uint8_t i_, j_;
    bool ready_;
};

} // namespace glide
