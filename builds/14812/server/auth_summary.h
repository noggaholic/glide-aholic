#pragma once
#include "glide/buffer.h"
#include <array>
#include <vector>
namespace glide {
namespace july14812 {
struct CharacterSummary {
    uint16_t map = 0;
    uint8_t retainedState = 0, level = 0, profession = 0;
    std::array<uint8_t, 45> appearance{};
    std::array<uint8_t, 10> retainedEquipment{};
    std::array<uint32_t, 6> armor{};
    std::array<std::array<uint16_t, 4>, 6> colors{};
    std::array<uint8_t, 16> retainedTail{};
};
struct ArmorSource {
    uint32_t item = 0;
    std::array<uint32_t, 4> colorIds{};
};
// Native July version7/153 bytes. Content membership is validated separately.
bool EncodeSummary(const CharacterSummary&, Buffer&);
bool DecodeSummary(ByteView, CharacterSummary&);
// The complete native palette is strictly sorted; index zero denotes its first entry.
bool DyeDataId(const std::vector<uint32_t>& palette, uint16_t index, uint32_t& out);
bool DyeIndex(const std::vector<uint32_t>& palette, uint32_t dataId, uint16_t& out);
bool SetArmor(CharacterSummary&, const std::array<ArmorSource, 6>&, const std::vector<uint32_t>& palette);
} // namespace july14812
} // namespace glide
