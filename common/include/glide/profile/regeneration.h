#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <utility>

namespace glide {
namespace modern {
// Explicit compatibility policy, NOT a recovered build-specific healing model:
// https://wiki.guildwars2.com/wiki/Regeneration :
// 5 + 1.5625 * source level + .125 * HealingPower per second. This profile
// supplies HealingPower=0 and uses the saved source character level.
// One strongest contribution per recipient, no offline healing or catch-up.
// Beta Symbol uses one-second289 grants: include their final opportunity.
// The modern alias keeps718 and its established exclusive endpoint unchanged.
template <unsigned Definition = 718, bool TickAtExpiration = false> class RegenerationState {
    static bool Covers(uint64_t end, uint64_t now) {
        return TickAtExpiration ? end >= now : end > now;
    }
    // A regular worker can observe the final nominal tick just after expiry.
    // Admit only its current one-second interval; never replay older ticks.
    static bool Current(uint64_t end, uint64_t now) {
        return TickAtExpiration ? (now <= end || now - end < 1000) : end > now;
    }
    struct Row {
        uint64_t id = 0, buff = 0, start = 0, end = 0;
        unsigned level = 0;
    };
    bool enabled_ = false, barrier_ = false;
    uint64_t anchor_ = 0, last_ = 0;
    std::map<unsigned, unsigned> levels_;
    std::map<unsigned, uint64_t> ends_;
    static bool Number(const std::string& text, uint64_t& value) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        value = 0;
        for (char ch : text) {
            if (ch < '0' || ch > '9' || value > (UINT64_MAX - unsigned(ch - '0')) / 10)
                return false;
            value = value * 10 + unsigned(ch - '0');
        }
        return true;
    }
    static std::vector<std::string> Split(const std::string& text, char separator) {
        std::vector<std::string> result;
        size_t begin = 0;
        for (;;) {
            const auto end = text.find(separator, begin);
            result.push_back(text.substr(begin, end == std::string::npos ? end : end - begin));
            if (end == std::string::npos)
                break;
            begin = end + 1;
        }
        return result;
    }
    static bool Decode(const std::string& text, bool& extended, uint64_t& next, uint64_t& anchor, uint64_t& last,
                       std::vector<Row>& rows) {
        rows.clear();
        anchor = last = 0;
        extended = false;
        next = 1;
        if (text.empty())
            return true;
        if (text.size() > 8192)
            return false;
        const auto bar = text.find('|');
        if (bar == std::string::npos)
            return false;
        const auto header = Split(text.substr(0, bar), ':');
        if (header.empty() || (header[0] != "1" && header[0] != "2"))
            return false;
        extended = header[0] == "2";
        if (header.size() != (extended ? 4u : 2u) || !Number(header[1], next) || !next || next > UINT32_MAX)
            return false;
        if (extended && (!Number(header[2], anchor) || !Number(header[3], last) || bool(anchor) != bool(last) ||
                         last < anchor || (last - anchor) % 1000))
            return false;
        std::set<uint64_t> ids;
        if (bar + 1 == text.size())
            return true;
        for (const auto& item : Split(text.substr(bar + 1), ',')) {
            const auto fields = Split(item, ':');
            if (fields.size() < 4)
                return false;
            uint64_t v[5]{};
            for (size_t i = 0; i < fields.size(); ++i)
                if (i >= 5 || !Number(fields[i], v[i]))
                    return false;
            const size_t count = extended && v[1] == Definition ? 5 : 4;
            if (fields.size() != count || rows.size() >= 64 || !v[0] || v[0] >= next || v[1] > UINT32_MAX || !v[2] ||
                v[3] <= v[2] || v[4] > 80 || !ids.insert(v[0]).second)
                return false;
            Row row;
            row.id = v[0];
            row.buff = v[1];
            row.start = v[2];
            row.end = v[3];
            row.level = unsigned(v[4]);
            rows.push_back(row);
        }
        return true;
    }
    static std::string RowsText(uint64_t next, const std::vector<Row>& rows) {
        auto text = "1:" + std::to_string(next) + '|';
        for (const auto& row : rows) {
            if (text.back() != '|')
                text += ',';
            text += std::to_string(row.id) + ':' + std::to_string(row.buff) + ':' + std::to_string(row.start) + ':' +
                    std::to_string(row.end);
        }
        return text;
    }

  public:
    // Reuse the canonical saved-row decoder for combat modifiers. Callers
    // supply the supported buff's duration ceiling. Count preserves individual
    // contributions; Active collapses overlapping contributions to one boolean.
    static bool StoredBuffCount(const std::string& text, unsigned buff, uint64_t utc, uint64_t maximumDuration,
                                unsigned& count) {
        count = 0;
        if (!utc || !buff || !maximumDuration)
            return false;
        bool extended = false;
        uint64_t next = 1, anchor = 0, last = 0;
        std::vector<Row> rows;
        if (!Decode(text, extended, next, anchor, last, rows))
            return false;
        for (const auto& row : rows)
            if (row.buff == buff) {
                if (row.end - row.start > maximumDuration)
                    return false;
                if (row.start <= utc && utc < row.end)
                    ++count;
            }
        return true;
    }
    static bool StoredBuffActive(const std::string& text, unsigned buff, uint64_t utc, uint64_t maximumDuration,
                                 bool& active) {
        active = false;
        unsigned count = 0;
        if (!StoredBuffCount(text, buff, utc, maximumDuration, count))
            return false;
        active = count > 0;
        return true;
    }
    void Forget(const std::set<unsigned>& ids) {
        for (auto id : ids) {
            levels_.erase(id);
            ends_.erase(id);
        }
    }
    bool Enabled() const {
        return enabled_;
    }
    bool Load(const std::string& text, std::string& legacy) {
        bool extended = false;
        uint64_t next = 1, anchor = 0, last = 0;
        std::vector<Row> rows;
        if (!Decode(text, extended, next, anchor, last, rows))
            return false;
        RegenerationState candidate;
        candidate.enabled_ = extended;
        candidate.barrier_ = extended;
        candidate.anchor_ = anchor;
        candidate.last_ = last;
        for (const auto& row : rows)
            if (row.buff == Definition && row.level) {
                candidate.levels_[unsigned(row.id)] = row.level;
                candidate.ends_[unsigned(row.id)] = row.end;
            }
        legacy = text.empty() ? std::string() : RowsText(next, rows);
        *this = std::move(candidate);
        return true;
    }
    static bool Legacy(const std::string& text, std::string& legacy) {
        RegenerationState state;
        return state.Load(text, legacy);
    }
    std::string Text(const std::string& legacy) const {
        if (!enabled_)
            return legacy;
        bool extended = false;
        uint64_t next = 1, anchor = 0, last = 0;
        std::vector<Row> rows;
        if (!Decode(legacy, extended, next, anchor, last, rows) || extended)
            return {};
        auto text = "2:" + std::to_string(next) + ':' + std::to_string(anchor_) + ':' + std::to_string(last_) + '|';
        for (const auto& row : rows) {
            if (text.back() != '|')
                text += ',';
            text += std::to_string(row.id) + ':' + std::to_string(row.buff) + ':' + std::to_string(row.start) + ':' +
                    std::to_string(row.end);
            if (row.buff == Definition) {
                const auto found = levels_.find(unsigned(row.id));
                text += ':' + std::to_string(found == levels_.end() ? 0 : found->second);
            }
        }
        return text.size() <= 8192 ? text : std::string();
    }
    bool Grant(unsigned instance, unsigned sourceLevel, uint64_t utc, uint64_t endsUtc = 0) {
        if (!instance || !utc || sourceLevel > 80 || (endsUtc && endsUtc <= utc))
            return false;
        if (!sourceLevel) {
            levels_.erase(instance);
            ends_.erase(instance);
            return true;
        }
        bool covered = false;
        for (const auto& item : ends_)
            if (Covers(item.second, utc))
                covered = true;
        enabled_ = true;
        levels_[instance] = sourceLevel;
        ends_[instance] = endsUtc ? endsUtc : UINT64_MAX;
        if (!anchor_ || !covered) {
            anchor_ = last_ = utc;
        }
        return true;
    }
    // Mutate only a disposable copy. Persist even changed=true/delta=0: a full
    // health recipient consumes the opportunity and a reconnect consumes none.
    bool Prepare(const std::string& legacy, uint64_t utc, float& health, float maximum, float& delta, bool& changed) {
        delta = 0;
        changed = false;
        if (!enabled_)
            return true;
        if (!utc || !std::isfinite(health) || !std::isfinite(maximum) || maximum <= 0 || health < 0 || health > maximum)
            return false;
        bool extended = false;
        uint64_t next = 1, unusedAnchor = 0, unusedLast = 0;
        std::vector<Row> rows;
        if (!Decode(legacy, extended, next, unusedAnchor, unusedLast, rows) || extended)
            return false;
        std::map<unsigned, unsigned> live;
        std::map<unsigned, uint64_t> ends;
        bool active = false;
        for (const auto& row : rows)
            if (row.buff == Definition && Current(row.end, utc)) {
                const auto found = levels_.find(unsigned(row.id));
                if (found != levels_.end()) {
                    live[unsigned(row.id)] = found->second;
                    ends[unsigned(row.id)] = row.end;
                    if (row.start <= utc)
                        active = true;
                }
            }
        ends_ = std::move(ends);
        if (live != levels_) {
            levels_ = std::move(live);
            changed = true;
        }
        if (!active) {
            if (anchor_ || last_) {
                anchor_ = last_ = 0;
                changed = true;
            }
            barrier_ = false;
            return true;
        }
        if (barrier_ || !anchor_ || utc < last_) {
            anchor_ = last_ = utc;
            barrier_ = false;
            changed = true;
            return true;
        }
        const auto nominal = utc - (utc - anchor_) % 1000;
        if (nominal <= last_)
            return true;
        unsigned strongest = 0;
        for (const auto& row : rows)
            if (row.buff == Definition && row.start <= nominal && Covers(row.end, nominal) && Current(row.end, utc)) {
                const auto found = levels_.find(unsigned(row.id));
                if (found != levels_.end())
                    strongest = (std::max)(strongest, found->second);
            }
        last_ = nominal;
        changed = true;
        if (health > 0 && strongest) {
            const float after = (std::min)(maximum, health + 5.0f + 1.5625f * static_cast<float>(strongest));
            delta = after - health;
            health = after;
        }
        return true;
    }
};
using Regeneration = RegenerationState<>;
} // namespace modern
} // namespace glide
