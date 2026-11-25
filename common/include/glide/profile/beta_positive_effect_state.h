#pragma once
#include "glide/profile/beta_symbol_data.h"
#include <set>
#include <vector>

namespace glide {
namespace beta {
constexpr uint32_t kFirstPositiveInstance = 0x40000000u;
struct StoredPositiveEffect {
    uint32_t id = 0, definition = 0;
    uint64_t start = 0, end = 0;
};
struct StoredPositiveEffects {
    std::string stance;
    Regeneration regeneration;
    uint32_t next = kFirstPositiveInstance;
    std::vector<StoredPositiveEffect> effects;
};
inline bool PositiveDecimal(const std::string& text, uint64_t& value) {
    if (text.empty() || (text.size() > 1 && text[0] == '0'))
        return false;
    value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9' || value > (UINT64_MAX - (ch - '0')) / 10)
            return false;
        value = value * 10 + static_cast<unsigned>(ch - '0');
    }
    return true;
}
// One canonical reader for the existing stance/boon/regen envelope. Profession
// and own duration validation stay with the controller that loads the effects.
inline bool ReadPositiveEffects(const std::string& text, StoredPositiveEffects& out) {
    const auto colon = text.find(':', 3);
    uint64_t length = 0;
    if (text.compare(0, 3, "b1:") || text.size() > 8192 || colon == std::string::npos ||
        !PositiveDecimal(text.substr(3, colon - 3), length) || !length || length > 160 ||
        length > text.size() - colon - 1)
        return false;
    StoredPositiveEffects parsed;
    parsed.stance = text.substr(colon + 1, static_cast<size_t>(length));
    const auto encoded = text.substr(colon + 1 + static_cast<size_t>(length));
    std::string rows;
    if ((encoded.compare(0, 2, "1:") && encoded.compare(0, 2, "2:")) || !parsed.regeneration.Load(encoded, rows))
        return false;
    const auto bar = rows.find('|');
    uint64_t next = 0;
    if (bar == std::string::npos || !PositiveDecimal(rows.substr(2, bar - 2), next) || next < kFirstPositiveInstance ||
        next > UINT32_MAX)
        return false;
    parsed.next = static_cast<uint32_t>(next);
    std::set<uint32_t> ids;
    size_t at = bar + 1;
    while (at < rows.size()) {
        const auto comma = rows.find(',', at), end = comma == std::string::npos ? rows.size() : comma;
        const auto row = rows.substr(at, end - at);
        uint64_t values[4]{};
        size_t begin = 0;
        for (unsigned i = 0; i < 4; ++i) {
            const auto stop = i == 3 ? row.size() : row.find(':', begin);
            if (stop == std::string::npos || !PositiveDecimal(row.substr(begin, stop - begin), values[i]))
                return false;
            begin = stop + 1;
        }
        if (values[0] < kFirstPositiveInstance || values[0] >= next || values[1] > UINT32_MAX || !values[1] ||
            !values[2] || values[3] <= values[2] || parsed.effects.size() >= 64 ||
            !ids.insert(static_cast<uint32_t>(values[0])).second)
            return false;
        parsed.effects.push_back(
            {static_cast<uint32_t>(values[0]), static_cast<uint32_t>(values[1]), values[2], values[3]});
        if (comma == std::string::npos)
            break;
        at = comma + 1;
        if (at == rows.size())
            return false;
    }
    out = std::move(parsed);
    return true;
}
} // namespace beta
} // namespace glide
