#pragma once
#include "glide/buffer.h"
#include <array>
#include <vector>
namespace glide {
namespace hot49284 {
struct CharacterSummary {
    uint16_t map = 0;
    uint8_t retainedState = 0, level = 0, profession = 0;
    std::array<uint8_t, 45> appearance{};
    std::array<uint8_t, 10> retainedEquipment{};
    std::array<uint32_t, 6> armor{};
    std::array<std::array<uint16_t, 4>, 6> colors{};
    std::array<uint8_t, 16> guild{};
};
struct AccountCapability {
    uint16_t id = 0, value = 0;
    std::array<uint32_t, 3> retained{};
};
struct AccountSnapshot {
    uint32_t request = 0;
    std::array<uint8_t, 16> user{}, selected{};
    uint32_t type = 0, agreement = 0, serverSeconds = 0, homeWorld = 0;
    uint32_t secondaryWorld = 0, worldClass = 0, authBaseSeconds = 0;
    std::array<uint8_t, 32> contentKey{}, retained{};
    std::vector<AccountCapability> capabilities;
};
// Own version-9 layout. Unknown fields are preserved, never borrowed from another build.
bool EncodeSummary(const CharacterSummary&, Buffer&);
bool DecodeSummary(ByteView, CharacterSummary&);
bool EncodeAccount(const AccountSnapshot&, Buffer&);
bool DecodeAccount(ByteView, AccountSnapshot&);
bool AuthTime(uint64_t unixSeconds, uint32_t& out);
uint32_t WorldRegion(uint32_t world);
uint32_t WorldClass(uint32_t world);
} // namespace hot49284
} // namespace glide
