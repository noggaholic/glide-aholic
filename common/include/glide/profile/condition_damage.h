#pragma once
#include "glide/profile/modern_conditions.h"
#include <map>
#include <cmath>
namespace glide {
namespace combat {
// Explicit compatible damage1 model, not recovered condition formulas.
// One current UTC-second opportunity, no catch-up. Entry/reconnect skips its
// current opportunity. Duration kinds contribute once, intensity per entry.
// Original fractional damage, source attributes and movement multipliers remain open.
struct ConditionDamage {
    enum Kind { None, Duration, Intensity };
    struct Hit {
        unsigned definition = 0, count = 0;
        float before = 0, after = 0;
    };
    struct Prepared {
        std::string clock;
        float health = 0;
        std::vector<Hit> hits;
        bool changed = false;
    };
    static bool Build(unsigned build) {
        return build == 14075 || build == 14422 || build == 14505 || build == 14812 || build == 49284 ||
               build == 167409 || build == 204489;
    }
    static Kind DamageKind(unsigned build, unsigned definition) {
        if (beta::IsOwnCondition(build, definition))
            return definition == 922 ? Intensity : (definition == 306 || definition == 1012) ? Duration : None;
        if (build == 49284 || build == 167409 || build == 204489) {
            if (definition == 736 || definition == 21632)
                return Intensity;
            if (definition == 737)
                return build == 49284 ? Duration : Intensity;
        }
        return None;
    }
    static bool LoadClock(unsigned build, const std::string& text, uint64_t& last, bool& present) {
        last = 0;
        present = false;
        if (text.empty())
            return true;
        if (!Build(build) || text.size() > 23 || text.compare(0, 3, "P1:") || text.size() == 3 ||
            (text.size() > 4 && text[3] == '0'))
            return false;
        for (size_t i = 3; i < text.size(); ++i) {
            const auto ch = text[i];
            if (ch < '0' || ch > '9' || last > (UINT64_MAX - unsigned(ch - '0')) / 10)
                return false;
            last = last * 10 + unsigned(ch - '0');
        }
        if (last % 1000)
            return false;
        present = true;
        return true;
    }
    static bool ValidClock(unsigned build, const std::string& text) {
        uint64_t last = 0;
        bool present = false;
        return LoadClock(build, text, last, present);
    }
    template <unsigned Dialect> static bool Rows(const std::string& text, std::vector<modern::ConditionEntry>& out) {
        modern::ConditionState<Dialect> state;
        if (!state.Load(text))
            return false;
        out = state.Entries();
        return true;
    }
    static bool Prepare(unsigned build, const std::string& clock, const std::string& conditions, uint64_t utc,
                        bool barrier, float health, float maximum, Prepared& out, bool convert = false) {
        out = {};
        out.health = health;
        uint64_t last = 0;
        bool present = false;
        if (!Build(build) || !utc || !std::isfinite(health) || !std::isfinite(maximum) || maximum <= 0 || health < 0 ||
            health > maximum || !LoadClock(build, clock, last, present))
            return false;
        std::vector<modern::ConditionEntry> rows;
        bool valid = false;
        switch (build) {
        case 14075:
        case 14422:
        case 14505:
        case 14812:
            valid = Rows<14505>(conditions, rows);
            break;
        case 49284:
            valid = Rows<49284>(conditions, rows);
            break;
        case 167409:
            valid = Rows<167409>(conditions, rows);
            break;
        case 204489:
            valid = Rows<204489>(conditions, rows);
            break;
        }
        if (!valid)
            return false;
        const auto nominal = utc - utc % 1000;
        bool tracked = false;
        for (const auto& row : rows)
            if (DamageKind(build, row.definition) != None && row.endsUtc >= nominal)
                tracked = true;
        if (!tracked) {
            out.changed = !clock.empty();
            return true;
        }
        const auto next = (std::max)(last, nominal);
        out.clock = "P1:" + std::to_string(next);
        out.changed = out.clock != clock;
        if (barrier || (present && nominal <= last))
            return true;
        std::map<unsigned, unsigned> due;
        for (const auto& row : rows)
            if (row.startsUtc < nominal && nominal <= row.endsUtc) {
                const auto kind = DamageKind(build, row.definition);
                if (kind == Intensity)
                    ++due[row.definition];
                else if (kind == Duration)
                    due[row.definition] = 1;
            }
        for (const auto& entry : due) {
            Hit hit;
            hit.definition = entry.first;
            hit.count = entry.second;
            hit.before = out.health;
            hit.after = hit.before > 0 ? (convert ? (std::min)(maximum, hit.before + static_cast<float>(hit.count))
                                                  : (std::max)(0.0f, hit.before - static_cast<float>(hit.count)))
                                       : hit.before;
            out.health = hit.after;
            if (hit.after != hit.before)
                out.hits.push_back(hit);
        }
        return true;
    }
};
} // namespace combat
} // namespace glide
