#include "auth_data.h"
#include <cstring>
#include <utility>
namespace glide {
namespace hot49284 {
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
    return s.level <= 80 && s.profession >= 1 && s.profession <= 9 && s.appearance[27] < 5 && s.appearance[28] < 16 &&
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
bool Var(Reader& reader, uint32_t& value) {
    return reader.VarU32(value) == DecodeStatus::Complete;
}
} // namespace
bool EncodeSummary(const CharacterSummary& s, Buffer& out) {
    if (!Valid(s))
        return false;
    Buffer b;
    if (!b.U8(9) || !b.U16(s.map) || !b.U8(s.retainedState) || !b.U8(s.level) || !b.U8(s.profession) ||
        !Write(b, s.appearance) || !Write(b, s.retainedEquipment))
        return false;
    for (size_t slot = 0; slot < s.armor.size(); ++slot) {
        if (!b.U32(s.armor[slot]))
            return false;
        for (auto color : s.colors[slot])
            if (!b.U16(color))
                return false;
    }
    if (!Write(b, s.guild) || !b.U32(Crc(b.View())))
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
    if (!r.U8(version) || version != 9 || !r.U16(s.map) || !r.U8(s.retainedState) || !r.U8(s.level) ||
        !r.U8(s.profession) || !Read(r, s.appearance) || !Read(r, s.retainedEquipment))
        return false;
    for (size_t slot = 0; slot < s.armor.size(); ++slot) {
        if (!r.U32(s.armor[slot]))
            return false;
        for (auto& color : s.colors[slot])
            if (!r.U16(color))
                return false;
    }
    if (!Read(r, s.guild) || !r.U32(checksum) || checksum != Crc(ByteView(bytes.data, 149)) || !Valid(s))
        return false;
    out = s;
    return true;
}
bool EncodeAccount(const AccountSnapshot& s, Buffer& out) {
    if (!s.serverSeconds || !s.authBaseSeconds || s.capabilities.size() > 50)
        return false;
    Buffer b;
    if (!b.U16(8) || !b.VarU32(s.request) || !Write(b, s.user) || !Write(b, s.selected))
        return false;
    for (auto value :
         {s.type, s.agreement, s.serverSeconds, s.homeWorld, s.secondaryWorld, s.worldClass, s.authBaseSeconds})
        if (!b.VarU32(value))
            return false;
    if (!Write(b, s.contentKey) || !b.U8(static_cast<uint8_t>(s.capabilities.size())))
        return false;
    for (const auto& c : s.capabilities) {
        if (!b.U16(c.id) || !b.U16(c.value))
            return false;
        for (auto value : c.retained)
            if (!b.VarU32(value))
                return false;
    }
    if (!Write(b, s.retained))
        return false;
    out = std::move(b);
    return true;
}
bool DecodeAccount(ByteView bytes, AccountSnapshot& out) {
    if (!bytes.Valid() || bytes.size > 1089)
        return false;
    Reader r(bytes);
    AccountSnapshot s;
    uint16_t id;
    uint8_t count;
    if (!r.U16(id) || id != 8 || !Var(r, s.request) || !Read(r, s.user) || !Read(r, s.selected))
        return false;
    for (auto value :
         {&s.type, &s.agreement, &s.serverSeconds, &s.homeWorld, &s.secondaryWorld, &s.worldClass, &s.authBaseSeconds})
        if (!Var(r, *value))
            return false;
    if (!s.serverSeconds || !s.authBaseSeconds || !Read(r, s.contentKey) || !r.U8(count) || count > 50)
        return false;
    for (unsigned i = 0; i < count; ++i) {
        AccountCapability c;
        if (!r.U16(c.id) || !r.U16(c.value))
            return false;
        for (auto& value : c.retained)
            if (!Var(r, value))
                return false;
        s.capabilities.push_back(c);
    }
    if (!Read(r, s.retained) || r.Remaining())
        return false;
    out = std::move(s);
    return true;
}
bool AuthTime(uint64_t seconds, uint32_t& out) {
    const uint64_t epoch = 978307200;
    if (seconds <= epoch || seconds - epoch > UINT32_MAX)
        return false;
    out = static_cast<uint32_t>(seconds - epoch);
    return true;
}
uint32_t WorldRegion(uint32_t world) {
    if (world >= 1 && world <= 64)
        return 1;
    if (world >= 65 && world <= 128)
        return 2;
    const auto region = world / 1000;
    return region >= 1 && region <= 9 ? region : 0;
}
uint32_t WorldClass(uint32_t world) {
    switch (WorldRegion(world)) {
    case 1:
        return 1;
    case 2:
        return 3;
    case 6:
    case 7:
        return 6;
    case 8:
        return 8;
    case 9:
        return 7;
    default:
        return 0;
    }
}
} // namespace hot49284
} // namespace glide
