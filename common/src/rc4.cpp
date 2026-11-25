#include "glide/rc4.h"
#include <Windows.h>
#include <utility>

namespace glide {

Rc4::Rc4() : state_{}, i_(0), j_(0), ready_(false) {}
Rc4::~Rc4() {
    SecureZeroMemory(state_, sizeof(state_));
}
bool Rc4::Initialize(ByteView key) {
    ready_ = false;
    SecureZeroMemory(state_, sizeof(state_));
    i_ = j_ = 0;
    if (!key.Valid() || key.size == 0)
        return false;
    for (unsigned n = 0; n < 256; ++n)
        state_[n] = static_cast<uint8_t>(n);
    unsigned j = 0;
    for (unsigned n = 0; n < 256; ++n) {
        j = (j + state_[n] + key.data[n % key.size]) & 255;
        std::swap(state_[n], state_[j]);
    }
    ready_ = true;
    return true;
}
bool Rc4::Apply(uint8_t* bytes, size_t size) {
    if (!ready_ || (!bytes && size))
        return false;
    for (size_t n = 0; n < size; ++n) {
        i_ = static_cast<uint8_t>(i_ + 1);
        j_ = static_cast<uint8_t>(j_ + state_[i_]);
        std::swap(state_[i_], state_[j_]);
        bytes[n] ^= state_[static_cast<uint8_t>(state_[i_] + state_[j_])];
    }
    return true;
}

} // namespace glide
