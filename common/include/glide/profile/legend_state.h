#pragma once
#include "glide/character.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <string>

namespace glide {
// Base legends only. Elite specializations require their own unlock state. Persistence and recharge rules are
// shared.
template <unsigned BaseFirst = 11, unsigned WaterMask = 12> class BasicLegendState {
    static_assert(BaseFirst <= 252 && WaterMask && WaterMask < 16, "Invalid base legend policy");
    static constexpr uint8_t DefaultFirst = BaseFirst + 2, DefaultSecond = BaseFirst + 3;
    bool enabled_ = false, restored_ = false, underwater_ = false;
    std::array<uint8_t, 2> land_{{0, 0}}, water_{{0, 0}};
    uint64_t deadline_ = 0, due_ = 0;
    static bool Number(const std::string& text, uint64_t& value) {
        if (text.empty() || text.size() > 20 || (text.size() > 1 && text[0] == '0'))
            return false;
        value = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || value > (UINT64_MAX - uint64_t(c - '0')) / 10)
                return false;
            value = value * 10 + uint64_t(c - '0');
        }
        return true;
    }

  public:
    static bool Base(unsigned legend) {
        return legend >= BaseFirst && legend <= BaseFirst + 3;
    }
    static bool Aquatic(unsigned legend) {
        return Base(legend) && (WaterMask & (1u << (legend - BaseFirst)));
    }
    bool Load(const Character& saved, unsigned profession) {
        enabled_ = profession == 9;
        underwater_ = false;
        land_ = {{static_cast<uint8_t>(enabled_ ? DefaultFirst : 0),
                  static_cast<uint8_t>(enabled_ && saved.level >= 11 ? DefaultSecond : 0)}};
        water_ = land_;
        deadline_ = 0;
        due_ = 0;
        restored_ = false;
        const auto at = saved.profileFields.find("legend_state");
        if (at == saved.profileFields.end() || at->second.empty())
            return true;
        if (!enabled_)
            return false;
        const auto& text = at->second;
        std::array<uint64_t, 5> values{};
        size_t begin = 0, count = 0;
        for (;;) {
            if (count == values.size())
                return false;
            const auto end = text.find(',', begin);
            if (!Number(text.substr(begin, end == std::string::npos ? end : end - begin), values[count++]))
                return false;
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        if (count != 3 && count != 5)
            return false;
        const auto validPair = [&](uint64_t first, uint64_t second, bool water) {
            return first <= 255 &&
                   (water ? Aquatic(static_cast<unsigned>(first)) : Base(static_cast<unsigned>(first))) &&
                   second <= 255 &&
                   (!second ||
                    (saved.level >= 11 && first != second &&
                     (water ? Aquatic(static_cast<unsigned>(second)) : Base(static_cast<unsigned>(second)))));
        };
        if (!validPair(values[0], values[1], false))
            return false;
        land_ = {{static_cast<uint8_t>(values[0]), static_cast<uint8_t>(values[1])}};
        if (count == 3) {
            // Upgrade older single-pair saves without discarding land choices.
            const uint8_t first = Aquatic(land_[0])          ? land_[0]
                                  : land_[1] == DefaultFirst ? DefaultSecond
                                                             : DefaultFirst;
            const uint8_t second = Aquatic(land_[1]) && land_[1] != first ? land_[1]
                                   : first == DefaultFirst                ? DefaultSecond
                                                                          : DefaultFirst;
            water_ = {{first, static_cast<uint8_t>(land_[1] ? second : 0)}};
        } else {
            if (!validPair(values[2], values[3], true))
                return false;
            water_ = {{static_cast<uint8_t>(values[2]), static_cast<uint8_t>(values[3])}};
        }
        deadline_ = values[count - 1];
        return (land_[1] || water_[1] || !deadline_);
    }
    bool Enabled() const {
        return enabled_;
    }
    bool Underwater() const {
        return underwater_;
    }
    void SetUnderwater(bool value) {
        underwater_ = enabled_ && value;
    }
    uint8_t First() const {
        return (underwater_ ? water_ : land_)[0];
    }
    uint8_t Second() const {
        return (underwater_ ? water_ : land_)[1];
    }
    uint8_t First(bool water) const {
        return (water ? water_ : land_)[0];
    }
    uint8_t Second(bool water) const {
        return (water ? water_ : land_)[1];
    }
    bool Restore(uint64_t now, uint64_t wall) {
        if (restored_ || !enabled_)
            return true;
        if (deadline_ && !wall)
            return false;
        const uint64_t remaining = deadline_ > wall ? (std::min)(deadline_ - wall, uint64_t(10000)) : 0;
        if (now > UINT64_MAX - remaining)
            return false;
        due_ = now + remaining;
        restored_ = true;
        return true;
    }
    uint32_t Remaining(uint64_t now) const {
        return due_ > now ? static_cast<uint32_t>((std::min)(due_ - now, uint64_t(10000))) : 0;
    }
    bool Select(unsigned slot, unsigned legend, unsigned level, uint64_t now) {
        if (!enabled_ || !restored_ || slot > 1 || !(underwater_ ? Aquatic(legend) : Base(legend)) ||
            (slot == 1 && level < 11) || Remaining(now))
            return false;
        auto& pair = underwater_ ? water_ : land_;
        if (legend == pair[1 - slot])
            return false;
        pair[slot] = static_cast<uint8_t>(legend);
        return true;
    }
    bool Swap(uint64_t now, uint64_t wall) {
        if (!enabled_ || !restored_ || !Second() || Remaining(now) || !wall || wall > UINT64_MAX - 10000 ||
            now > UINT64_MAX - 10000)
            return false;
        auto& pair = underwater_ ? water_ : land_;
        std::swap(pair[0], pair[1]);
        deadline_ = wall + 10000;
        due_ = now + 10000;
        return true;
    }
    std::string Text() const {
        return std::to_string(land_[0]) + "," + std::to_string(land_[1]) + "," + std::to_string(water_[0]) + "," +
               std::to_string(water_[1]) + "," + std::to_string(deadline_);
    }
};
using LegendState = BasicLegendState<>;
} // namespace glide
