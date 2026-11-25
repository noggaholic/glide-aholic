#pragma once
#include "glide/guid.h"
#include "glide/character.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/modern_conditions.h"
#include <cmath>
namespace glide {
namespace combat {
// 26679 records; use the2s player-combat variant, not today's changed API skill.
struct ForcedEngagement {
    enum : unsigned { Skill = 26679, Slow = 26766, Taunt = 27705, Duration = 2000, Slot = 2 };
    static bool Build(unsigned b) {
        return b == 49284 || b == 167409 || b == 204489;
    }
    static unsigned Legend(unsigned b) {
        return b == 49284 ? 13 : b == 167409 ? 16 : 17;
    }
    static unsigned Range(unsigned b) {
        return b == 49284 ? 1200 : 900;
    }
    static unsigned Activation(unsigned b, bool water) {
        return b == 49284 ? 1300 : water ? 650 : 600;
    }
    static unsigned Animation(unsigned b, bool water) {
        return b == 49284 ? 1730 : water ? 1833 : 1233;
    }
    static unsigned Recharge(unsigned b) {
        return b == 49284 ? 3000 : 15000;
    }
    static bool BasicRequest(unsigned build, uint16_t actor, uint16_t target, unsigned skill, uint32_t token,
                             Buffer& out) {
        if (!Build(build) || !actor || !target || target == actor || target == 57344 || !(token & 0x80000000u) ||
            !(token & 0x7fffffffu) || (!skill && build != 49284))
            return false;
        Buffer b;
        if (!b.U16(build == 49284 ? 18 : 23) || !b.U16(build == 49284 ? actor : 57344) || !b.VarU32(1) || !b.U8(3) ||
            !b.VarU32(token) || !b.U8(build == 49284 ? 6 : 10) || !b.U8(build == 49284 ? 0x15 : 35) || !b.U32(target))
            return false;
        if (build != 49284 && !b.U32(skill))
            return false;
        if (!b.U8(5))
            return false;
        out = std::move(b);
        return true;
    }
    static bool RestrictedRequest(unsigned build, ByteView input) {
        Reader r(input);
        uint16_t id = 0;
        if (!r.U16(id))
            return false;
        return build == 49284 ? (id == 18 || id == 109 || id == 9)
                              : (build == 167409 || build == 204489) && (id == 23 || id == 180 || id == 192 || id == 9);
    }
    template <unsigned Build> static bool Conditions(const WorldActionContext& c, uint64_t nominal, std::string& text) {
        modern::ConditionState<Build> state;
        if (!nominal || nominal > c.wallNow || nominal > UINT64_MAX - Duration || !state.Load(c.conditionState))
            return false;
        state.Expire(c.wallNow);
        if (nominal + Duration > c.wallNow && !state.Add(Slow, nominal, Duration))
            return false;
        text = state.Text();
        return true;
    }
    static bool Contains(unsigned b, const CatalogPosition& origin, const CatalogPosition& p) {
        const double distance =
            32 * std::hypot(std::hypot(double(p.x) - origin.x, double(p.y) - origin.y), double(p.z) - origin.z);
        return std::isfinite(distance) && distance <= Range(b);
    }
};
class TauntState {
    uint64_t starts_ = 0, ends_ = 0;
    std::string source_;
    static bool Number(const std::string& s, uint64_t& n) {
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
    static bool Source(const std::string& s) {
        const auto colon = s.find(':');
        if (colon == std::string::npos)
            return false;
        Guid guid;
        uint64_t owner = 0;
        return Guid::Parse(s.substr(0, colon), guid) && guid.Text() == s.substr(0, colon) &&
               Number(s.substr(colon + 1), owner) && owner;
    }

  public:
    // Ordinary conditions occupy only offsets1..0x7fff in this native namespace.
    static const uint32_t Instance = 0xffff8000u;
    static std::string Field(const Character& c) {
        const auto i = c.profileFields.find("taunt_state");
        return i == c.profileFields.end() ? std::string() : i->second;
    }
    bool Load(unsigned build, const std::string& text) {
        starts_ = ends_ = 0;
        source_.clear();
        if (text.empty())
            return true;
        if (!ForcedEngagement::Build(build) || text.size() > 128 || text.compare(0, 3, "T1:"))
            return false;
        const auto a = text.find(':', 3), z = a == std::string::npos ? a : text.find(':', a + 1);
        uint64_t start = 0, end = 0;
        if (a == std::string::npos || z == std::string::npos || !Number(text.substr(3, a - 3), start) ||
            !Number(text.substr(a + 1, z - a - 1), end) || !start || end <= start ||
            end - start != ForcedEngagement::Duration || !Source(text.substr(z + 1)))
            return false;
        starts_ = start;
        ends_ = end;
        source_ = text.substr(z + 1);
        return true;
    }
    bool Apply(uint64_t nominal, uint64_t now, const std::string& source) {
        if (!nominal || nominal > now || nominal > UINT64_MAX - ForcedEngagement::Duration || !Source(source))
            return false;
        if (nominal + ForcedEngagement::Duration <= now || nominal < starts_)
            return true;
        starts_ = nominal;
        ends_ = nominal + ForcedEngagement::Duration;
        source_ = source;
        return true;
    }
    bool Active(uint64_t utc) const {
        return starts_ <= utc && utc < ends_;
    }
    const std::string& SourceIdentity() const {
        return source_;
    }
    std::string Text() const {
        return ends_ ? "T1:" + std::to_string(starts_) + ":" + std::to_string(ends_) + ":" + source_ : std::string();
    }
    bool Create(unsigned build, uint16_t target, uint16_t caster, uint64_t utc, unsigned level, Buffer& out) const {
        return ForcedEngagement::Build(build) && Active(utc) && target && caster && target != caster && level <= 80 &&
               out.U16(build == 49284    ? 497
                       : build == 167409 ? 713
                                         : 735) &&
               out.VarU32(target) && out.VarU32(caster) && out.VarU32(ForcedEngagement::Taunt) &&
               out.VarU32(Instance) && out.VarU32(static_cast<uint32_t>(ends_ - utc)) &&
               out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    static bool Remove(unsigned build, uint16_t target, uint16_t caster, Buffer& out) {
        return ForcedEngagement::Build(build) && target && caster &&
               out.U16(build == 49284    ? 500
                       : build == 167409 ? 717
                                         : 739) &&
               out.VarU32(target) && out.VarU32(caster) && out.VarU32(Instance);
    }
};
} // namespace combat
} // namespace glide
