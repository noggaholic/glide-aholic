#include "auth_route.h"
#include "auth_bytes.h"
#include <utility>
namespace glide {
namespace july14812 {
bool EncodeSelect(const SelectResult& s, Buffer& out) {
    Buffer b;
    if (!b.U16(3) || !b.VarU32(s.request) || !b.VarU32(s.map) || !detail::Write(b, s.endpoint))
        return false;
    for (auto value : s.error)
        if (!b.VarU32(value))
            return false;
    out = std::move(b);
    return true;
}
bool DecodeSelect(ByteView input, SelectResult& out) {
    if (!input.Valid() || input.size > 60)
        return false;
    Reader r(input);
    SelectResult s;
    uint16_t id;
    if (!r.U16(id) || id != 3 || !detail::Var(r, s.request) || !detail::Var(r, s.map) || !detail::Read(r, s.endpoint))
        return false;
    for (auto& value : s.error)
        if (!detail::Var(r, value))
            return false;
    if (r.Remaining())
        return false;
    out = s;
    return true;
}
bool EncodeRedirect(const RedirectResult& s, Buffer& out) {
    Buffer b;
    if (!b.U16(20) || !b.VarU32(s.request))
        return false;
    for (size_t i = 0; i < 3; ++i)
        if (!b.VarU32(s.routing[i]))
            return false;
    if (!detail::Write(b, s.endpoint) || !b.VarU32(s.routing[3]))
        return false;
    out = std::move(b);
    return true;
}
bool DecodeRedirect(ByteView input, RedirectResult& out) {
    if (!input.Valid() || input.size > 55)
        return false;
    Reader r(input);
    RedirectResult s;
    uint16_t id;
    if (!r.U16(id) || id != 20 || !detail::Var(r, s.request))
        return false;
    for (size_t i = 0; i < 3; ++i)
        if (!detail::Var(r, s.routing[i]))
            return false;
    if (!detail::Read(r, s.endpoint) || !detail::Var(r, s.routing[3]) || r.Remaining())
        return false;
    out = s;
    return true;
}
bool LoopbackEndpoint(uint16_t port, std::array<uint8_t, 28>& out) {
    if (!port)
        return false;
    std::array<uint8_t, 28> value{};
    value[0] = 2;
    value[2] = static_cast<uint8_t>(port >> 8);
    value[3] = static_cast<uint8_t>(port);
    value[4] = 127;
    value[7] = 1;
    out = value;
    return true;
}
} // namespace july14812
} // namespace glide
