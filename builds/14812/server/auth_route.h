#pragma once
#include "glide/buffer.h"
#include <array>
namespace glide {
namespace july14812 {
struct SelectResult {
    uint32_t request = 0, map = 0;
    std::array<uint8_t, 28> endpoint{};
    std::array<uint32_t, 4> error{};
};
struct RedirectResult {
    uint32_t request = 0;
    std::array<uint32_t, 4> routing{};
    std::array<uint8_t, 28> endpoint{};
};
// Raw July structures only: referrals require separate account/world authorization.
bool EncodeSelect(const SelectResult&, Buffer&);
bool DecodeSelect(ByteView, SelectResult&);
bool EncodeRedirect(const RedirectResult&, Buffer&);
bool DecodeRedirect(ByteView, RedirectResult&);
bool LoopbackEndpoint(uint16_t port, std::array<uint8_t, 28>&);
} // namespace july14812
} // namespace glide
