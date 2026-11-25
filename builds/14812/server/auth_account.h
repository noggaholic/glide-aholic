#pragma once
#include "glide/buffer.h"
#include <array>
#include <vector>
namespace glide {
namespace july14812 {
struct AccountCapability {
    uint16_t id = 0, value = 0;
    std::array<uint32_t, 3> retained{};
};
struct AccountSnapshot {
    uint32_t request = 0;
    std::array<uint8_t, 16> user{}, selected{};
    uint32_t type = 0, agreement = 0, homeWorld = 0, worldClass = 0;
    std::array<uint8_t, 32> contentKey{}, retained{};
    std::vector<AccountCapability> capabilities;
};
// July Auth8 has four scalars; no account clocks or implicit defaults.
bool EncodeAccount(const AccountSnapshot&, Buffer&);
bool DecodeAccount(ByteView, AccountSnapshot&);
bool ValidAccountType(uint32_t);
uint16_t CapabilityValue(const AccountSnapshot&, uint16_t);
uint32_t CharacterSlots(const AccountSnapshot&);
uint8_t AcceptedAgreement(const AccountSnapshot&);
} // namespace july14812
} // namespace glide
