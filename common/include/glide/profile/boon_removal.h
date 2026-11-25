#pragma once
#include "glide/character.h"
#include "glide/buffer.h"
#include <algorithm>
#include <set>
#include <vector>
namespace glide {
namespace combat {
struct LiveBoon {
    unsigned instance = 0, definition = 0;
    uint64_t starts = 0, ends = 0;
    bool announced = false;
};
// Remember stripped contributions until their original deadlines. A maintained
// emitter can then replace its missing contribution with a fresh native ID.
struct BoonRemovals {
    std::vector<LiveBoon> rows;
    static bool Known(unsigned buff) {
        switch (buff) {
        case 717:
        case 718:
        case 719:
        case 725:
        case 726:
        case 740:
        case 743:
        case 873:
        case 1122:
        case 1187:
        case 26980:
        case 30328:
            return true;
        default:
            return false;
        }
    }
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
    static std::string Field(const Character& c) {
        const auto it = c.profileFields.find("removed_boons");
        return it == c.profileFields.end() ? std::string() : it->second;
    }
    bool Load(const std::string& text) {
        rows.clear();
        if (text.empty())
            return true;
        if (text.size() > 8192 || text.compare(0, 3, "R1:") || text.size() == 3)
            return false;
        size_t at = 3;
        std::set<unsigned> ids;
        while (at < text.size()) {
            const auto comma = text.find(',', at), end = comma == std::string::npos ? text.size() : comma;
            uint64_t v[3]{};
            for (unsigned i = 0; i < 3; ++i) {
                const auto stop = i == 2 ? end : text.find(':', at);
                if (stop == std::string::npos || stop > end || !Number(text.substr(at, stop - at), v[i]))
                    return false;
                at = stop + 1;
            }
            if (!v[0] || v[0] >= 0xffff0000u || v[1] > UINT32_MAX || !Known(unsigned(v[1])) || !v[2] ||
                rows.size() >= 256 || !ids.insert(unsigned(v[0])).second)
                return false;
            LiveBoon row;
            row.instance = unsigned(v[0]);
            row.definition = unsigned(v[1]);
            row.ends = v[2];
            rows.push_back(row);
            if (comma == std::string::npos)
                break;
            if (at == text.size())
                return false;
        }
        return true;
    }
    bool Contains(unsigned id, unsigned definition, uint64_t utc = 0) const {
        for (const auto& r : rows)
            if (r.instance == id && r.definition == definition && (!utc || utc < r.ends))
                return true;
        return false;
    }
    static bool Removed(const std::string& text, unsigned id, unsigned definition, uint64_t utc = 0) {
        BoonRemovals r;
        return id && r.Load(text) && r.Contains(id, definition, utc);
    }
    std::string Text() const {
        std::string out;
        for (const auto& r : rows) {
            out += out.empty() ? "R1:" : ",";
            out += std::to_string(r.instance) + ":" + std::to_string(r.definition) + ":" + std::to_string(r.ends);
        }
        return out;
    }
    bool Select(std::vector<LiveBoon> live, unsigned maximum, uint64_t utc, std::vector<LiveBoon>& removed,
                uint64_t startedBefore = 0) {
        removed.clear();
        if (!utc || maximum > 3)
            return false;
        rows.erase(std::remove_if(rows.begin(), rows.end(), [&](const LiveBoon& r) { return r.ends <= utc; }),
                   rows.end());
        std::sort(live.begin(), live.end(),
                  [](const LiveBoon& a, const LiveBoon& b) { return a.instance < b.instance; });
        std::set<unsigned> kinds, ids;
        for (const auto& r : live) {
            if (!Known(r.definition) || !r.instance || r.starts > (startedBefore ? startedBefore : utc) ||
                r.ends <= utc || Contains(r.instance, r.definition, utc))
                continue;
            if (!kinds.count(r.definition) && kinds.size() == maximum)
                continue;
            kinds.insert(r.definition);
            if (!ids.insert(r.instance).second)
                return false;
            removed.push_back(r);
        }
        if (rows.size() + removed.size() > 256)
            return false;
        rows.insert(rows.end(), removed.begin(), removed.end());
        return true;
    }
    bool SelectContribution(const std::vector<LiveBoon>& live, unsigned definition, uint64_t utc,
                            std::vector<LiveBoon>& removed, uint64_t nominal) {
        std::vector<LiveBoon> selected;
        for (const auto& row : live)
            if (row.definition == definition && row.instance && row.starts <= nominal && row.ends > utc &&
                !Contains(row.instance, row.definition, utc)) {
                if (selected.empty() || row.instance < selected.front().instance)
                    selected = {row};
            }
        return Select(selected, 1, utc, removed, nominal);
    }
    static std::set<unsigned> Instances(const std::vector<LiveBoon>& rows) {
        std::set<unsigned> out;
        for (const auto& r : rows)
            out.insert(r.instance);
        return out;
    }
};
template <unsigned Build>
bool RemovedBoonMessages(uint16_t owner, uint16_t peer, const std::vector<LiveBoon>& removed, Buffer& own,
                         Buffer& remote) {
    for (const auto& row : removed)
        if (row.announced) {
            const uint16_t id = Build == 49284 ? 500 : Build == 167409 ? 717 : 739;
            if (!own.U16(id) || !own.VarU32(owner) || !own.VarU32(owner) || !own.VarU32(row.instance) ||
                !remote.U16(id) || !remote.VarU32(peer) || !remote.VarU32(peer) || !remote.VarU32(row.instance))
                return false;
        }
    return true;
}
} // namespace combat
} // namespace glide
