#pragma once
#include "glide/character.h"
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
namespace glide {
namespace combat {
// Explicit per-hit policy; no invented native buff/entity ID. Existing provisional damage uses reference1000.
struct AssassinSiphon {
    uint64_t starts = 0, ends = 0, last = 0;
    unsigned remaining = 0;
    static bool Build(unsigned build) {
        return build == 49284 || build == 167409 || build == 204489;
    }
    static std::string Field(const Character& c) {
        const auto at = c.profileFields.find("assassin_siphon");
        return at == c.profileFields.end() ? std::string() : at->second;
    }
    static bool Number(const std::string& s, uint64_t& n) {
        if (s.empty() || (s.size() > 1 && s[0] == '0'))
            return false;
        n = 0;
        for (char c : s) {
            if (c < '0' || c > '9' || n > (UINT64_MAX - unsigned(c - '0')) / 10)
                return false;
            n = n * 10 + unsigned(c - '0');
        }
        return true;
    }
    bool Load(const std::string& text) {
        *this = {};
        if (text.empty())
            return true;
        if (text.size() > 96 || text.compare(0, 3, "A1:"))
            return false;
        std::vector<uint64_t> values;
        size_t at = 3;
        for (;;) {
            const auto end = text.find(':', at);
            uint64_t n = 0;
            if (!Number(text.substr(at, end == std::string::npos ? end : end - at), n))
                return false;
            values.push_back(n);
            if (end == std::string::npos)
                break;
            at = end + 1;
        }
        if (values.size() != 4 || !values[0] || values[0] > UINT64_MAX - 15000 || values[1] != values[0] + 15000 ||
            values[2] > 6 || (values[3] && (values[3] < values[0] || values[3] >= values[1])) ||
            ((values[2] < 6) != (values[3] != 0)))
            return false;
        starts = values[0];
        ends = values[1];
        remaining = static_cast<unsigned>(values[2]);
        last = values[3];
        return true;
    }
    bool Load(unsigned build, const Character& c) {
        const auto text = Field(c);
        if (!text.empty()) {
            const auto p = c.profileFields.find("profession");
            if (!Build(build) || p == c.profileFields.end() || p->second != "9")
                return false;
        }
        return Load(text);
    }
    bool Arm(uint64_t utc) {
        if (!utc || utc > UINT64_MAX - 15000)
            return false;
        starts = utc;
        ends = utc + 15000;
        remaining = 6;
        last = 0;
        return true;
    }
    std::string Text() const {
        return starts ? "A1:" + std::to_string(starts) + ":" + std::to_string(ends) + ":" + std::to_string(remaining) +
                            ":" + std::to_string(last)
                      : std::string();
    }
    bool Ready(uint64_t utc) const {
        return remaining && starts <= utc && utc < ends && (!last || (utc >= last && utc - last >= 500));
    }
    bool Consume(uint64_t utc) {
        if (!Ready(utc))
            return false;
        --remaining;
        last = utc;
        return true;
    }
    static float Healing(unsigned build, unsigned level) {
        return Build(build) && level && level <= 80
                   ? 8.0f + std::round(float(level) * level * (build == 49284 ? 0.125f : 0.11875f))
                   : 0;
    }
    static float Damage(unsigned build, unsigned level) {
        return Build(build) && level && level <= 80
                   ? (8.0f + std::round(float(level) * level * (build == 49284 ? 0.125f : 0.15f))) / 1000.0f
                   : 0;
    }
};
} // namespace combat
} // namespace glide
