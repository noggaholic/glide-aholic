#pragma once
#include "glide/profile/dragon_facet_data.h"
#include "glide/profile/modern_energy.h"
#include "glide/profile/modern_condition_data.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

namespace glide {
namespace modern {
// Durable independent facets, embedded in Hammers' H3 envelope. This is the
// existing maintained ledger's multi-entry case, not a second resource ledger.
template <unsigned Build> class DragonFacets {
  public:
    struct Contribution {
        std::string identity;
        unsigned instance = 0;
        uint64_t ends = 0;
    };
    struct Entry {
        unsigned base = 0, marker = 0, own = 0;
        uint64_t starts = 0, at = 0, lastPulse = 0, ownEnds = 0;
        bool active = false, announced = false;
        std::vector<Contribution> allies;
    };
    std::vector<Entry> entries;
    uint64_t revision = 1;
    static bool Decimal(const std::string& text, uint64_t& out) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        out = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || out > (UINT64_MAX - (c - '0')) / 10)
                return false;
            out = out * 10 + c - '0';
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
    static std::string Hex(const std::string& text) {
        static const char digits[] = "0123456789abcdef";
        std::string result;
        for (unsigned char c : text) {
            result += digits[c >> 4];
            result += digits[c & 15];
        }
        return result;
    }
    static bool Unhex(const std::string& text, std::string& result) {
        if (text.empty() || text.size() > 128 || text.size() % 2)
            return false;
        result.clear();
        for (size_t i = 0; i < text.size(); i += 2) {
            unsigned byte = 0;
            for (unsigned j = 0; j < 2; ++j) {
                const char c = text[i + j];
                if (c >= '0' && c <= '9')
                    byte = byte * 16 + c - '0';
                else if (c >= 'a' && c <= 'f')
                    byte = byte * 16 + c - 'a' + 10;
                else
                    return false;
            }
            if (byte < 32 || byte > 126)
                return false;
            result += static_cast<char>(byte);
        }
        return true;
    }
    static std::string Guid(const std::string& identity) {
        return identity.substr(0, identity.find(':'));
    }
    Entry* Find(unsigned base) {
        for (auto& e : entries)
            if (e.base == base)
                return &e;
        return nullptr;
    }
    const Entry* Find(unsigned base) const {
        for (const auto& e : entries)
            if (e.base == base)
                return &e;
        return nullptr;
    }
    bool Present() const {
        for (const auto& e : entries)
            if (e.marker)
                return true;
        return false;
    }
    int Rate() const {
        int result = 5;
        for (const auto& e : entries)
            if (e.marker && e.active)
                result += FacetData<Build>::RuleFor(e.base).upkeep;
        return result;
    }
    void Cancel() {
        for (auto& e : entries) {
            e.marker = 0;
            e.starts = e.at = e.lastPulse = 0;
            e.active = e.announced = false;
        }
        ++revision;
    }
    bool Schedule(unsigned base, unsigned marker, const WorldActionContext& c, bool water) {
        const auto rule = FacetData<Build>::RuleFor(base, water);
        if (!FacetData<Build>::IsBase(base) || !marker || marker >= kFirstConditionInstance || !c.wallNow ||
            c.wallNow > UINT64_MAX - rule.activation || c.now > UINT64_MAX - rule.activation)
            return false;
        auto* current = Find(base);
        if (current && current->marker)
            return false;
        for (const auto& e : entries)
            if (e.marker == marker || e.own == marker)
                return false;
        if (!current) {
            if (entries.size() >= 5)
                return false;
            Entry e;
            e.base = base;
            entries.push_back(e);
            std::sort(entries.begin(), entries.end(), [](const Entry& a, const Entry& b) { return a.base < b.base; });
            current = Find(base);
        }
        current->marker = marker;
        current->starts = c.wallNow + rule.activation;
        current->at = c.now + rule.activation;
        current->active = current->announced = false;
        current->lastPulse = 0;
        ++revision;
        return true;
    }
    bool Consume(unsigned base, uint64_t now, Energy<Build>& energy) {
        auto* e = Find(base);
        if (!e || !e->marker || !e->active)
            return false;
        e->marker = 0;
        e->starts = e->at = e->lastPulse = 0;
        e->active = e->announced = false;
        if (!energy.SetFacetRate(now, Rate()))
            return false;
        ++revision;
        return true;
    }
    // Caller supplies a candidate and publishes neither energy nor markers yet.
    bool Advance(const WorldActionContext& c, Energy<Build>& energy, bool cancel, bool& changed) {
        changed = false;
        if (!c.wallNow)
            return false;
        if (cancel) {
            if (Present()) {
                uint64_t exhausted = 0;
                if (energy.Exhaustion(c.now, exhausted) && !energy.SetFacetRate(exhausted, 5))
                    return false;
                Cancel();
                if (!energy.SetFacetRate(c.now, 5))
                    return false;
                changed = true;
            }
            return true;
        }
        for (;;) {
            uint64_t boundary = UINT64_MAX;
            for (const auto& e : entries)
                if (e.marker && !e.active && e.at <= c.now)
                    boundary = (std::min)(boundary, e.at);
            uint64_t exhausted = 0;
            const auto until = boundary == UINT64_MAX ? c.now : boundary;
            if (energy.Exhaustion(until, exhausted)) {
                if (!energy.SetFacetRate(exhausted, 5))
                    return false;
                Cancel();
                changed = true;
                return true;
            }
            if (boundary == UINT64_MAX)
                break;
            for (auto& e : entries)
                if (e.marker && !e.active && e.at == boundary)
                    e.active = true;
            if (!energy.SetFacetRate(boundary, Rate()))
                return false;
            ++revision;
            changed = true;
        }
        return true;
    }
    static unsigned Accrue(unsigned amount, int rate, uint64_t elapsed) {
        const auto delta = int64_t((std::min)(elapsed, uint64_t(100000))) * rate;
        return unsigned((std::max)(int64_t(0), (std::min)(int64_t(50000), int64_t(amount) + delta)));
    }
    bool Restore(unsigned amount, int rate, uint64_t saved, const WorldActionContext& c, Energy<Build>& energy,
                 bool& changed) {
        if (!saved || !c.wallNow || c.wallNow < saved || rate != Rate())
            return false;
        changed = false;
        uint64_t cursor = saved;
        for (;;) {
            uint64_t boundary = c.wallNow;
            bool activation = false;
            for (const auto& e : entries)
                if (e.marker && !e.active && e.starts <= boundary) {
                    boundary = e.starts;
                    activation = true;
                }
            if (boundary < cursor)
                return false;
            const auto elapsed = boundary - cursor;
            const auto drain = rate < 0 ? (uint64_t(amount) + unsigned(-rate) - 1) / unsigned(-rate) : UINT64_MAX;
            if (rate < 0 && elapsed >= drain) {
                amount = Accrue(0, 5, c.wallNow - cursor - drain);
                Cancel();
                rate = 5;
                changed = true;
                break;
            }
            amount = Accrue(amount, rate, elapsed);
            cursor = boundary;
            if (!activation)
                break;
            for (auto& e : entries)
                if (e.marker && !e.active && e.starts == boundary)
                    e.active = true;
            rate = Rate();
            changed = true;
            ++revision;
        }
        for (auto& e : entries)
            if (e.marker) {
                const auto delay = e.starts > c.wallNow ? e.starts - c.wallNow : 0;
                if (c.now > UINT64_MAX - delay)
                    return false;
                e.at = e.active ? c.now : c.now + delay;
                e.announced = false;
            }
        return energy.RestoreFacetLedger(amount, rate, c.now);
    }
    std::string Text() const {
        std::string result;
        for (const auto& e : entries) {
            if (!result.empty())
                result += ';';
            result += std::to_string(e.base) + ',' + std::to_string(e.marker) + ',' + std::to_string(e.starts) + ',' +
                      std::to_string(e.active ? 1 : 0) + ',' + std::to_string(e.lastPulse) + ',' +
                      std::to_string(e.own) + ',' + std::to_string(e.ownEnds) + ',';
            for (size_t i = 0; i < e.allies.size(); ++i) {
                if (i)
                    result += '+';
                const auto& a = e.allies[i];
                result += Hex(a.identity) + '~' + std::to_string(a.instance) + '~' + std::to_string(a.ends);
            }
        }
        return result;
    }
    bool Load(const std::string& text, uint64_t saved) {
        entries.clear();
        if (text.empty())
            return true;
        if (text.size() > 4000)
            return false;
        unsigned previous = 0;
        std::set<unsigned> markers, own;
        for (const auto& part : Split(text, ';')) {
            const auto fields = Split(part, ',');
            if (fields.size() != 8 || entries.size() >= 5)
                return false;
            uint64_t n[7]{};
            for (unsigned i = 0; i < 7; ++i)
                if (!Decimal(fields[i], n[i]))
                    return false;
            if (n[0] > UINT32_MAX || n[0] <= previous || !FacetData<Build>::IsBase(unsigned(n[0])) ||
                n[1] >= kFirstConditionInstance || n[3] > 1 || n[5] >= kFirstConditionInstance)
                return false;
            Entry e;
            e.base = unsigned(n[0]);
            e.marker = unsigned(n[1]);
            e.starts = n[2];
            e.active = n[3] != 0;
            e.lastPulse = n[4];
            e.own = unsigned(n[5]);
            e.ownEnds = n[6];
            if (e.marker) {
                if (!e.starts || !markers.insert(e.marker).second)
                    return false;
                if (e.active ? (e.starts > saved) : (e.starts <= saved || e.starts - saved > 700))
                    return false;
                if (e.lastPulse &&
                    (!e.active || e.lastPulse < e.starts || e.lastPulse > saved || (e.lastPulse - e.starts) % 3000))
                    return false;
            } else if (e.starts || e.active || e.lastPulse)
                return false;
            if (bool(e.own) != bool(e.ownEnds) || (e.own && !own.insert(e.own).second) ||
                (e.ownEnds > saved && e.ownEnds - saved > FacetData<Build>::RuleFor(e.base).duration))
                return false;
            std::set<std::string> identities;
            if (!fields[7].empty())
                for (const auto& item : Split(fields[7], '+')) {
                    const auto fields2 = Split(item, '~');
                    Contribution a;
                    uint64_t instance = 0;
                    if (fields2.size() != 3 || e.allies.size() >= 4 || !Unhex(fields2[0], a.identity) ||
                        !identities.insert(Guid(a.identity)).second || !Decimal(fields2[1], instance) || !instance ||
                        instance >= kFirstConditionInstance || !Decimal(fields2[2], a.ends) || !a.ends ||
                        (a.ends > saved && a.ends - saved > FacetData<Build>::RuleFor(e.base).duration))
                        return false;
                    a.instance = unsigned(instance);
                    e.allies.push_back(std::move(a));
                }
            previous = e.base;
            entries.push_back(std::move(e));
        }
        for (unsigned id : markers)
            if (own.count(id))
                return false;
        return true;
    }
    bool AllocatorValid(const std::string& tail) const {
        const auto bar = tail.find('|');
        uint64_t next = 0;
        if (tail.compare(0, 2, "1:") || bar == std::string::npos || !Decimal(tail.substr(2, bar - 2), next) || !next ||
            next > kFirstConditionInstance)
            return false;
        for (const auto& e : entries)
            if (e.marker >= next || e.own >= next)
                return false;
        for (const auto& row : Split(tail.substr(bar + 1), ','))
            if (!row.empty()) {
                const auto colon = row.find(':');
                uint64_t id = 0;
                if (colon == std::string::npos || !Decimal(row.substr(0, colon), id))
                    return false;
                for (const auto& e : entries)
                    if (e.marker && id == e.marker)
                        return false;
            }
        return true;
    }
    FacetPulseStatus Prepare(const WorldActionContext& c, const std::vector<WorldConditionActor>& allies,
                             FacetPulsePlan& out) const {
        if (!c.wallNow)
            return FacetPulseStatus::Invalid;
        FacetPulsePlan plan;
        plan.revision = revision;
        plan.utc = c.wallNow;
        std::vector<std::pair<double, const WorldConditionActor*>> eligible;
        std::set<std::string> identities;
        for (const auto& a : allies) {
            if (!a.agent || a.agent == c.peerAgent || a.identity.empty() || a.identity.size() > 64 ||
                !identities.insert(a.identity).second)
                continue;
            const double x = (double(a.position.x) - c.position.x) * 32, y = (double(a.position.y) - c.position.y) * 32,
                         z = (double(a.position.z) - c.position.z) * 32;
            const double distance = x * x + y * y + z * z;
            if (std::isfinite(distance) && distance <= 360000)
                eligible.push_back({distance, &a});
        }
        std::sort(eligible.begin(), eligible.end(),
                  [](const std::pair<double, const WorldConditionActor*>& a,
                     const std::pair<double, const WorldConditionActor*>& b) {
                      return a.first != b.first ? a.first < b.first : a.second->identity < b.second->identity;
                  });
        // One common nearest-four recipient set keeps the server batch bounded.
        if (eligible.size() > 4)
            eligible.resize(4);
        for (const auto& e : entries)
            if (e.marker && e.active) {
                if (c.wallNow < e.starts)
                    return FacetPulseStatus::Invalid;
                const auto nominal = c.wallNow - (c.wallNow - e.starts) % 3000;
                if (e.lastPulse && nominal <= e.lastPulse)
                    continue;
                const auto rule = FacetData<Build>::RuleFor(e.base);
                if (nominal > UINT64_MAX - rule.duration)
                    return FacetPulseStatus::Invalid;
                FacetPulseSelection selected;
                selected.base = e.base;
                selected.nominal = nominal;
                const auto ends = nominal + rule.duration;
                if (ends > c.wallNow) {
                    // Each might pulse is an independent contribution. H3 remembers
                    // only its latest ID; older entries retain their durable deadlines.
                    // No original global stack cap or combat attributes are inferred.
                    const bool independent = rule.boon == 740;
                    ReceivedWorldEffect own;
                    own.definition = rule.boon;
                    own.endsUtc = ends;
                    own.refreshInstance = !independent && e.ownEnds > c.wallNow ? e.own : 0;
                    selected.ownIndex = int(plan.own.size());
                    plan.own.push_back(own);
                    unsigned occupied = 0;
                    for (const auto& old : e.allies)
                        if (old.ends > c.wallNow)
                            ++occupied;
                    for (const auto& pair : eligible) {
                        const auto& a = *pair.second;
                        const Contribution* old = nullptr;
                        for (const auto& value : e.allies)
                            if (Guid(value.identity) == Guid(a.identity) && value.ends > c.wallNow) {
                                old = &value;
                                break;
                            }
                        if (old && old->identity != a.identity)
                            continue; // Same GUID, new owner: wait out old contribution.
                        if (!old && occupied >= 4)
                            continue;
                        WorldReceivedEffectTarget request;
                        request.agent = a.agent;
                        request.identity = a.identity;
                        request.definition = rule.boon;
                        request.refreshInstance = !independent && old ? old->instance : 0;
                        request.endsUtc = ends;
                        selected.allyIndices.push_back(unsigned(plan.allies.size()));
                        plan.allies.push_back(request);
                        if (!old)
                            ++occupied;
                    }
                }
                plan.selected.push_back(std::move(selected));
            }
        if (plan.selected.empty())
            return FacetPulseStatus::None;
        out = std::move(plan);
        return FacetPulseStatus::Ready;
    }
    bool Commit(const FacetPulsePlan& plan, const std::vector<unsigned>& own, const std::vector<unsigned>& allies) {
        if (plan.revision != revision || !plan.utc || plan.selected.empty() || own.size() != plan.own.size() ||
            allies.size() != plan.allies.size())
            return false;
        std::set<unsigned> ids;
        for (size_t i = 0; i < own.size(); ++i)
            if (!own[i] || own[i] >= kFirstConditionInstance || !ids.insert(own[i]).second ||
                (plan.own[i].refreshInstance && plan.own[i].refreshInstance != own[i]))
                return false;
        for (size_t i = 0; i < allies.size(); ++i)
            if (!allies[i] || allies[i] >= kFirstConditionInstance ||
                (plan.allies[i].refreshInstance && plan.allies[i].refreshInstance != allies[i]))
                return false;
        auto next = *this;
        for (auto& e : next.entries)
            e.allies.erase(std::remove_if(e.allies.begin(), e.allies.end(),
                                          [&](const Contribution& a) { return a.ends <= plan.utc; }),
                           e.allies.end());
        for (const auto& selected : plan.selected) {
            auto* e = next.Find(selected.base);
            if (!e || !e->marker || !e->active || selected.nominal < e->starts || selected.nominal > plan.utc ||
                (selected.nominal - e->starts) % 3000 || (e->lastPulse && selected.nominal <= e->lastPulse))
                return false;
            e->lastPulse = selected.nominal;
            if (selected.ownIndex >= 0) {
                const auto i = unsigned(selected.ownIndex);
                if (i >= own.size())
                    return false;
                e->own = own[i];
                e->ownEnds = plan.own[i].endsUtc;
            }
            for (unsigned i : selected.allyIndices) {
                if (i >= allies.size())
                    return false;
                const auto& request = plan.allies[i];
                Contribution* found = nullptr;
                for (auto& a : e->allies)
                    if (a.identity == request.identity) {
                        found = &a;
                        break;
                    }
                if (!found) {
                    if (e->allies.size() >= 4)
                        return false;
                    Contribution a;
                    a.identity = request.identity;
                    e->allies.push_back(a);
                    found = &e->allies.back();
                }
                found->instance = allies[i];
                found->ends = request.endsUtc;
            }
        }
        ids.clear();
        for (const auto& e : next.entries)
            if (e.own && !ids.insert(e.own).second)
                return false;
        for (const auto& e : next.entries)
            if (e.marker && ids.count(e.marker))
                return false;
        ++next.revision;
        *this = std::move(next);
        return true;
    }
};
} // namespace modern
} // namespace glide
