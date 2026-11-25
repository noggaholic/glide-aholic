#include "auth_account.h"
#include "auth_bytes.h"
#include <utility>
namespace glide {
namespace july14812 {
bool EncodeAccount(const AccountSnapshot& s, Buffer& out) {
    if (s.capabilities.size() > 50)
        return false;
    Buffer b;
    if (!b.U16(8) || !b.VarU32(s.request) || !detail::Write(b, s.user) || !detail::Write(b, s.selected))
        return false;
    for (auto value : {s.type, s.agreement, s.homeWorld, s.worldClass})
        if (!b.VarU32(value))
            return false;
    if (!detail::Write(b, s.contentKey) || !b.U8(static_cast<uint8_t>(s.capabilities.size())))
        return false;
    for (const auto& cap : s.capabilities) {
        if (!b.U16(cap.id) || !b.U16(cap.value))
            return false;
        for (auto value : cap.retained)
            if (!b.VarU32(value))
                return false;
    }
    if (!detail::Write(b, s.retained))
        return false;
    out = std::move(b);
    return true;
}
bool DecodeAccount(ByteView input, AccountSnapshot& out) {
    if (!input.Valid() || input.size > 1074)
        return false;
    Reader r(input);
    AccountSnapshot s;
    uint16_t id;
    uint8_t count;
    if (!r.U16(id) || id != 8 || !detail::Var(r, s.request) || !detail::Read(r, s.user) || !detail::Read(r, s.selected))
        return false;
    for (auto value : {&s.type, &s.agreement, &s.homeWorld, &s.worldClass})
        if (!detail::Var(r, *value))
            return false;
    if (!detail::Read(r, s.contentKey) || !r.U8(count) || count > 50)
        return false;
    for (unsigned i = 0; i < count; ++i) {
        AccountCapability cap;
        if (!r.U16(cap.id) || !r.U16(cap.value))
            return false;
        for (auto& value : cap.retained)
            if (!detail::Var(r, value))
                return false;
        s.capabilities.push_back(cap);
    }
    if (!detail::Read(r, s.retained) || r.Remaining())
        return false;
    out = std::move(s);
    return true;
}
bool ValidAccountType(uint32_t type) {
    return type < 8;
}

uint16_t CapabilityValue(const AccountSnapshot& s, uint16_t id) {
    if (s.capabilities.size() > 50)
        return 0;
    for (const auto& cap : s.capabilities)
        if (cap.id == id)
            return cap.value;
    return 0;
}
uint32_t CharacterSlots(const AccountSnapshot& s) {
    return 5u + CapabilityValue(s, 1);
}
uint8_t AcceptedAgreement(const AccountSnapshot& s) {
    return static_cast<uint8_t>(s.agreement);
}
} // namespace july14812
} // namespace glide
