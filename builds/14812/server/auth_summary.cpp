#include "auth_summary.h"
#include <algorithm>
#include <cstring>
#include <utility>
namespace glide {
namespace july14812 {
namespace {
uint32_t Crc(ByteView bytes) {
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < bytes.size; ++i) {
        crc ^= bytes.data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0);
    }
    return ~crc;
}
bool Valid(const CharacterSummary& s) {
    return s.level <= 80 && s.profession >= 1 && s.profession <= 8 && s.appearance[27] < 5 && s.appearance[28] < 16 &&
           s.appearance[29] < 3;
}
template <size_t N> bool Read(Reader& reader, std::array<uint8_t, N>& out) {
    ByteView bytes;
    if (!reader.Bytes(N, bytes))
        return false;
    std::memcpy(out.data(), bytes.data, N);
    return true;
}
template <size_t N> bool Write(Buffer& buffer, const std::array<uint8_t, N>& bytes) {
    return buffer.Append(ByteView(bytes.data(), N));
}
bool PaletteValid(const std::vector<uint32_t>& palette) {
    if (palette.empty() || palette.size() > 65536)
        return false;
    for (size_t i = 1; i < palette.size(); ++i)
        if (palette[i] <= palette[i - 1])
            return false;
    return true;
}
bool FindDye(const std::vector<uint32_t>& palette, uint32_t dataId, uint16_t& out) {
    const auto at = std::lower_bound(palette.begin(), palette.end(), dataId);
    if (at == palette.end() || *at != dataId)
        return false;
    out = static_cast<uint16_t>(at - palette.begin());
    return true;
}
} // namespace
bool EncodeSummary(const CharacterSummary& s, Buffer& out) {
    if (!Valid(s))
        return false;
    Buffer b;
    if (!b.U8(7) || !b.U16(s.map) || !b.U8(s.retainedState) || !b.U8(s.level) || !b.U8(s.profession) ||
        !Write(b, s.appearance) || !Write(b, s.retainedEquipment))
        return false;
    for (size_t slot = 0; slot < s.armor.size(); ++slot) {
        if (!b.U32(s.armor[slot]))
            return false;
        for (auto color : s.colors[slot])
            if (!b.U16(color))
                return false;
    }
    if (!Write(b, s.retainedTail) || !b.U32(Crc(b.View())))
        return false;
    out = std::move(b);
    return true;
}
bool DecodeSummary(ByteView bytes, CharacterSummary& out) {
    if (!bytes.Valid() || bytes.size != 153)
        return false;
    Reader r(bytes);
    CharacterSummary s;
    uint8_t version;
    uint32_t checksum;
    if (!r.U8(version) || version != 7 || !r.U16(s.map) || !r.U8(s.retainedState) || !r.U8(s.level) ||
        !r.U8(s.profession) || !Read(r, s.appearance) || !Read(r, s.retainedEquipment))
        return false;
    for (size_t slot = 0; slot < s.armor.size(); ++slot) {
        if (!r.U32(s.armor[slot]))
            return false;
        for (auto& color : s.colors[slot])
            if (!r.U16(color))
                return false;
    }
    if (!Read(r, s.retainedTail) || !r.U32(checksum) || checksum != Crc(ByteView(bytes.data, 149)) || !Valid(s))
        return false;
    out = s;
    return true;
}
bool DyeDataId(const std::vector<uint32_t>& palette, uint16_t index, uint32_t& out) {
    if (!PaletteValid(palette) || index >= palette.size())
        return false;
    out = palette[index];
    return true;
}
bool DyeIndex(const std::vector<uint32_t>& palette, uint32_t dataId, uint16_t& out) {
    return PaletteValid(palette) && FindDye(palette, dataId, out);
}
bool SetArmor(CharacterSummary& summary, const std::array<ArmorSource, 6>& armor,
              const std::vector<uint32_t>& palette) {
    if (!PaletteValid(palette))
        return false;
    auto next = summary;
    for (size_t slot = 0; slot < armor.size(); ++slot) {
        next.armor[slot] = armor[slot].item;
        for (size_t channel = 0; channel < 4; ++channel)
            if (!FindDye(palette, armor[slot].colorIds[channel], next.colors[slot][channel]))
                return false;
    }
    summary = next;
    return true;
}
} // namespace july14812
} // namespace glide
