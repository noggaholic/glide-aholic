#pragma once
#include "glide/buffer.h"
#include <string>

namespace glide {
namespace modern {
// Exploration policy for an unmodified resource: cap100, dodge50, refill5/s. Millipoints avoid losing fractional
// refill on saves/reconnects. No evasion, collision, traits or combat modifiers are modeled by this ledger.
// Accepted dodge evasion is a separate transient DodgeEvasion window.
class EnduranceLedger {
  public:
    static const unsigned Maximum = 100000, DodgeCost = 50000, RefillPerMillisecond = 5;
    bool Load(const std::string& text) {
        points_ = Maximum;
        utc_ = 0;
        if (text.empty())
            return true;
        if (text.size() > 40 || text.compare(0, 2, "1:"))
            return false;
        const auto colon = text.find(':', 2);
        uint64_t points = 0, utc = 0;
        if (colon == std::string::npos || !Decimal(text.substr(2, colon - 2), points) || points > Maximum ||
            !Decimal(text.substr(colon + 1), utc) || !utc)
            return false;
        points_ = static_cast<unsigned>(points);
        utc_ = utc;
        return true;
    }
    bool Advance(uint64_t utc) {
        if (!utc)
            return false;
        if (!utc_) {
            utc_ = utc;
            return true;
        }
        if (utc <= utc_)
            return true; // A backwards wall clock cannot grant refill.
        const uint64_t elapsed = utc - utc_;
        const unsigned refill =
            elapsed >= Maximum / RefillPerMillisecond ? Maximum : static_cast<unsigned>(elapsed) * RefillPerMillisecond;
        points_ = refill > Maximum - points_ ? Maximum : points_ + refill;
        utc_ = utc;
        return true;
    }
    bool SpendDodge() {
        if (points_ < DodgeCost)
            return false;
        points_ -= DodgeCost;
        return true;
    }
    void Gain(unsigned points) {
        if (points >= 100 || points * 1000 > Maximum - points_)
            points_ = Maximum;
        else
            points_ += points * 1000;
    }
    void GainMillipoints(unsigned points) {
        points_ = points > Maximum - points_ ? Maximum : points_ + points;
    }
    unsigned Points() const {
        return points_ / 1000;
    }
    unsigned Millipoints() const {
        return points_;
    }
    uint64_t SavedUtc() const {
        return utc_;
    }
    std::string Text() const {
        return utc_ ? "1:" + std::to_string(points_) + ":" + std::to_string(utc_) : std::string();
    }

  private:
    static bool Decimal(const std::string& text, uint64_t& out) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        out = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || out > (UINT64_MAX - (c - '0')) / 10)
                return false;
            out = out * 10 + static_cast<unsigned>(c - '0');
        }
        return true;
    }
    unsigned points_ = Maximum;
    uint64_t utc_ = 0;
};
inline bool EnduranceCurrent(unsigned build, uint16_t actor, unsigned points, Buffer& out) {
    const uint16_t id = build == 49284 ? 169 : build == 167409 ? 216 : build == 204489 ? 218 : 0;
    return id && actor && points <= 100 && out.U16(id) && out.VarU32(actor) && out.VarU32(points);
}
// C11 from one ordinary V on each modern client (2026-11-16).
// This reads the command header only; callers must validate the complete
// packet with their build codec before accepting or relaying it.
inline bool ModernDodgeToken(ByteView input, uint32_t& out) {
    Reader r(input);
    uint16_t id = 0, actor = 0;
    uint32_t sequence = 0, token = 0;
    uint8_t state = 0, size = 0;
    ByteView command;
    if (!input.Valid() || !r.U16(id) || id != 11 || !r.U16(actor) || actor != 57344 ||
        r.VarU32(sequence) != DecodeStatus::Complete || !sequence || !r.U8(state) || state != 3 ||
        r.VarU32(token) != DecodeStatus::Complete || !(token & 0x80000000u) || !(token & 0x7fffffffu) || !r.U8(size) ||
        size != 3 || !r.Bytes(size, command) || command.data[0] != 0x6c || command.data[1] || command.data[2])
        return false;
    out = token & 0x7fffffffu;
    return true;
}
} // namespace modern
} // namespace glide
