#pragma once
#include "glide/profile/character_codec.h"
#include "glide/profile/world_actions.h"
#include <cmath>
#include <string>
namespace glide {
namespace combat {

struct JadeWinds {
    enum : unsigned { Skill = 28406, Cost = 50, Range = 600, Targets = 5, Duration = 3000, Animation = 2033 };
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static unsigned Legend(unsigned build) {
        return build == 49284 ? 12 : build == 167409 ? 15 : 16;
    }
    static unsigned Activation(unsigned build, bool water) {
        return build == 49284 ? 1200 : water ? 1100 : 1200;
    }
    static unsigned Recharge(unsigned build) {
        return build == 49284 ? 0 : 5000;
    }
    static bool Contains(const CatalogPosition& origin, const CatalogPosition& point) {
        const double distance = 32 * std::hypot(std::hypot(double(point.x) - origin.x, double(point.y) - origin.y),
                                                double(point.z) - origin.z);
        return std::isfinite(distance) && distance <= Range;
    }
};
// Admit only the known self-targeted29209 envelope through the control gate.
// The ordinary controller still validates profession, legend, cost and identity.
inline bool StunBreakRequest(unsigned build, uint16_t actor, ByteView input) {
    if (!JadeWinds::Build(build) || !actor)
        return false;
    Reader r(input);
    uint16_t id = 0, body = 0;
    uint32_t tick = 0, token = 0, target = 0, skill = 0;
    uint8_t flags = 0, size = 0, command = 0, slot = 0;
    ByteView bytes;
    if (!r.U16(id) || id != (build == 49284 ? 18 : 23) || !r.U16(body) || body != (build == 49284 ? actor : 57344u) ||
        r.VarU32(tick) != DecodeStatus::Complete || !r.U8(flags) || r.VarU32(token) != DecodeStatus::Complete ||
        !(token & 0x80000000u) || !(token & 0x7fffffffu) || !r.U8(size) || size != (build == 49284 ? 6 : 10) ||
        !r.Bytes(size, bytes) || r.Remaining())
        return false;
    Reader p(bytes);
    if (!p.U8(command) || command != (build == 49284 ? 0x15 : 35) || !p.U32(target) ||
        (target && target != (build == 49284 ? actor : 57344u)))
        return false;
    if (build != 49284 && (!p.U32(skill) || skill != 29209))
        return false;
    return p.U8(slot) && slot == (build == 49284 ? 3 : 1) && !p.Remaining();
}
// A control is not an ordinary condition: cleansing conditions cannot remove it.
// Keep the authored UTC deadline across retry/re-entry; a later stun extends only
// to its own deadline, never stacks durations or restarts an expired application.
class StunState {
  public:
    static std::string Field(const Character& c) {
        const auto i = c.profileFields.find("control_state");
        return i == c.profileFields.end() ? std::string() : i->second;
    }
    bool Load(unsigned build, const std::string& text) {
        start_ = end_ = 0;
        if (text.empty())
            return true;
        if (!JadeWinds::Build(build) || text.size() > 60 || text.compare(0, 9, "C1:28406:"))
            return false;
        const auto colon = text.find(':', 9);
        uint64_t start = 0, end = 0;
        if (colon == std::string::npos || !Decimal(text.substr(9, colon - 9), start) ||
            !Decimal(text.substr(colon + 1), end) || !start || end <= start || end - start != JadeWinds::Duration)
            return false;
        start_ = start;
        end_ = end;
        return true;
    }
    bool Apply(uint64_t nominal, uint64_t now) {
        if (!nominal || nominal > now || nominal > UINT64_MAX - JadeWinds::Duration)
            return false;
        const auto end = nominal + JadeWinds::Duration;
        if (end <= now)
            return true;
        if (end > end_) {
            start_ = nominal;
            end_ = end;
        }
        return true;
    }
    bool Active(uint64_t now) const {
        return start_ <= now && now < end_;
    }
    std::string Text() const {
        return end_ ? "C1:28406:" + std::to_string(start_) + ":" + std::to_string(end_) : std::string();
    }

  private:
    static bool Decimal(const std::string& s, uint64_t& n) {
        n = 0;
        if (s.empty() || (s.size() > 1 && s[0] == '0'))
            return false;
        for (char c : s) {
            if (c < '0' || c > '9' || n > (UINT64_MAX - unsigned(c - '0')) / 10)
                return false;
            n = n * 10 + unsigned(c - '0');
        }
        return true;
    }
    uint64_t start_ = 0, end_ = 0;
};
} // namespace combat
} // namespace glide
