#pragma once
#include "glide/character.h"
#include "glide/profile/gaze_darkness.h"
namespace glide {
namespace combat {
// Separate from cleansable conditions and removable boons; never renew on retry.
class RevealState {
    uint64_t starts_ = 0, ends_ = 0;
    static bool Number(const std::string& text, uint64_t& n) {
        n = 0;
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        for (char c : text) {
            if (c < '0' || c > '9' || n > (UINT64_MAX - unsigned(c - '0')) / 10)
                return false;
            n = n * 10 + unsigned(c - '0');
        }
        return true;
    }

  public:
    static const uint32_t Instance = 0xffff8001u; // Separate from ordinary conditions and source-bound Taunt.
    static std::string Field(const Character& ch) {
        const auto i = ch.profileFields.find("reveal_state");
        return i == ch.profileFields.end() ? std::string() : i->second;
    }
    bool Load(unsigned build, const std::string& text) {
        starts_ = ends_ = 0;
        if (text.empty())
            return true;
        if (!GazeDarkness::Build(build) || text.size() > 48 || text.compare(0, 3, "L1:"))
            return false;
        const auto bar = text.find(':', 3);
        uint64_t a = 0, b = 0;
        if (bar == std::string::npos || !Number(text.substr(3, bar - 3), a) || !Number(text.substr(bar + 1), b) || !a ||
            b <= a || b - a != GazeDarkness::Duration)
            return false;
        starts_ = a;
        ends_ = b;
        return true;
    }
    bool Apply(uint64_t nominal, uint64_t now) {
        uint64_t end = 0;
        if (!GazeDarkness::Deadline(nominal, now, end))
            return false;
        if (end > now && nominal >= starts_) {
            starts_ = nominal;
            ends_ = end;
        }
        return true;
    }
    bool Active(uint64_t utc) const {
        return starts_ <= utc && utc < ends_;
    }
    bool Create(unsigned build, uint16_t actor, uint64_t utc, unsigned level, Buffer& out) const {
        return GazeDarkness::Build(build) && actor && level <= 80 && Active(utc) &&
               out.U16(build == 49284    ? 497
                       : build == 167409 ? 713
                                         : 735) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(GazeDarkness::Reveal) && out.VarU32(Instance) &&
               out.VarU32(static_cast<uint32_t>(ends_ - utc)) && out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    static bool Remove(unsigned build, uint16_t actor, Buffer& out) {
        return GazeDarkness::Build(build) && actor &&
               out.U16(build == 49284    ? 500
                       : build == 167409 ? 717
                                         : 739) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(Instance);
    }
    uint64_t Ends() const {
        return ends_;
    }
    std::string Text() const {
        return ends_ ? "L1:" + std::to_string(starts_) + ":" + std::to_string(ends_) : std::string();
    }
};
} // namespace combat
} // namespace glide
