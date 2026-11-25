#pragma once
#include "glide/profile/modern_condition_data.h"
#include "glide/profile/world_actions.h"
#include <algorithm>
#include <set>
#include <vector>

namespace glide {
namespace modern {
struct ConditionEntry {
    unsigned instance = 0, definition = 0;
    uint64_t startsUtc = 0, endsUtc = 0;
    bool operator==(const ConditionEntry& other) const {
        return instance == other.instance && definition == other.definition && startsUtc == other.startsUtc &&
               endsUtc == other.endsUtc;
    }
};
template <unsigned Build> class ConditionState {
  public:
    static const unsigned MaximumDuration = 3600000, MaximumEntries = 64, InstanceLimit = 0x8000;
    bool Load(const std::string& text) {
        entries_.clear();
        next_ = 1;
        if (text.empty())
            return true;
        const auto bar = text.find('|');
        uint64_t next = 0;
        if (text.size() > 8192 || text.compare(0, 2, "1:") || bar == std::string::npos ||
            !Decimal(text.substr(2, bar - 2), next) || !next || next > InstanceLimit)
            return false;
        next_ = static_cast<unsigned>(next);
        size_t begin = bar + 1;
        unsigned previous = 0;
        while (begin < text.size()) {
            const auto comma = text.find(',', begin), end = comma == std::string::npos ? text.size() : comma;
            uint64_t fields[4]{};
            size_t cursor = begin;
            for (unsigned field = 0; field < 4; ++field) {
                const auto stop = field == 3 ? end : text.find(':', cursor);
                if (stop == std::string::npos || stop > end ||
                    !Decimal(text.substr(cursor, stop - cursor), fields[field]))
                    return false;
                cursor = stop + 1;
            }
            if (fields[0] <= previous || fields[0] >= next_ || fields[1] > UINT32_MAX ||
                !IsConditionDefinition(Build, static_cast<unsigned>(fields[1])) || !fields[2] ||
                fields[3] <= fields[2] || fields[3] - fields[2] > MaximumDuration || entries_.size() >= MaximumEntries)
                return false;
            ConditionEntry entry;
            entry.instance = static_cast<unsigned>(fields[0]);
            entry.definition = static_cast<unsigned>(fields[1]);
            entry.startsUtc = fields[2];
            entry.endsUtc = fields[3];
            entries_.push_back(entry);
            previous = entry.instance;
            if (comma == std::string::npos)
                break;
            begin = comma + 1;
            if (begin == text.size())
                return false;
        }
        return true;
    }
    bool Add(unsigned definition, uint64_t utc, unsigned duration) {
        if (!IsConditionDefinition(Build, definition) || !utc || !duration || duration > MaximumDuration ||
            utc > UINT64_MAX - duration || entries_.size() >= MaximumEntries || next_ >= InstanceLimit)
            return false;
        ConditionEntry entry;
        entry.instance = next_++;
        entry.definition = definition;
        entry.startsUtc = utc;
        entry.endsUtc = utc + duration;
        entries_.push_back(entry);
        return true;
    }
    // The caller selects eligible allies and instances and durably commits both
    // resulting states together. This operation alone does not perform I/O.
    bool TransferTo(ConditionState& recipient, const std::vector<unsigned>& instances, uint64_t utc) {
        if (this == &recipient || !utc)
            return false;
        ConditionState source = *this, destination = recipient;
        std::set<unsigned> selected;
        for (unsigned instance : instances) {
            if (!selected.insert(instance).second)
                return false;
            const auto found = std::find_if(source.entries_.begin(), source.entries_.end(),
                                            [&](const ConditionEntry& entry) { return entry.instance == instance; });
            if (found == source.entries_.end() || found->startsUtc > utc || found->endsUtc <= utc)
                return false;
            const auto remaining = found->endsUtc - utc;
            if (remaining > MaximumDuration ||
                !destination.Add(found->definition, utc, static_cast<unsigned>(remaining)))
                return false;
        }
        source.entries_.erase(
            std::remove_if(source.entries_.begin(), source.entries_.end(),
                           [&](const ConditionEntry& entry) { return selected.count(entry.instance) != 0; }),
            source.entries_.end());
        *this = std::move(source);
        recipient = std::move(destination);
        return true;
    }
    void Expire(uint64_t utc) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const ConditionEntry& entry) { return entry.endsUtc <= utc; }),
                       entries_.end());
    }
    unsigned ActiveKinds(uint64_t utc) const {
        std::set<unsigned> kinds;
        for (const auto& entry : entries_)
            if (entry.startsUtc <= utc && utc < entry.endsUtc)
                kinds.insert(entry.definition);
        return static_cast<unsigned>(kinds.size());
    }
    unsigned RemoveKinds(unsigned maximum, uint64_t utc) {
        // Deterministic local policy: oldest applied kinds first, all their stacks.
        // No claim that the unavailable production server uses this ordering.
        std::set<unsigned> removed;
        for (const auto& entry : entries_)
            if (entry.startsUtc <= utc && entry.endsUtc > utc && removed.size() < maximum)
                removed.insert(entry.definition);
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const ConditionEntry& entry) {
                                          return entry.startsUtc <= utc && removed.count(entry.definition) != 0;
                                      }),
                       entries_.end());
        return static_cast<unsigned>(removed.size());
    }
    // Compatible attack consumption: oldest active contribution only.
    unsigned ConsumeOne(unsigned definition, uint64_t utc) {
        const auto found = std::find_if(entries_.begin(), entries_.end(), [&](const ConditionEntry& entry) {
            return entry.definition == definition && entry.startsUtc <= utc && utc < entry.endsUtc;
        });
        if (found == entries_.end())
            return 0;
        const unsigned instance = found->instance;
        entries_.erase(found);
        return instance;
    }
    unsigned RemoveDefinitions(const std::vector<unsigned>& definitions, uint64_t utc) {
        // Pure selective removal: every active matching stack, with unrelated,
        // expired and future entries and their exact timestamps left untouched.
        const auto before = entries_.size();
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const ConditionEntry& entry) {
                                          return entry.startsUtc <= utc && utc < entry.endsUtc &&
                                                 std::find(definitions.begin(), definitions.end(), entry.definition) !=
                                                     definitions.end();
                                      }),
                       entries_.end());
        return static_cast<unsigned>(before - entries_.size());
    }
    std::string Text() const {
        if (next_ == 1 && entries_.empty())
            return {};
        std::string text = "1:" + std::to_string(next_) + "|";
        for (size_t i = 0; i < entries_.size(); ++i) {
            const auto& entry = entries_[i];
            if (i)
                text += ',';
            text += std::to_string(entry.instance) + ':' + std::to_string(entry.definition) + ':' +
                    std::to_string(entry.startsUtc) + ':' + std::to_string(entry.endsUtc);
        }
        return text;
    }
    const std::vector<ConditionEntry>& Entries() const {
        return entries_;
    }

  private:
    static bool Decimal(const std::string& text, uint64_t& value) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        value = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || value > (UINT64_MAX - (c - '0')) / 10)
                return false;
            value = value * 10 + (c - '0');
        }
        return true;
    }
    unsigned next_ = 1;
    std::vector<ConditionEntry> entries_;
};

template <unsigned Build> class ConditionActions {
  public:
    static_assert(Build == 14505 || Build == 49284 || Build == 167409 || Build == 204489,
                  "Unsupported condition dialect");
    // Four beta condition sets are independently verified identical. Their
    // runtime controllers share the14505 ledger and pass their own wire IDs.
    explicit ConditionActions(uint16_t owner = 57344,
                              uint16_t create = Build == 14505    ? 313
                                                : Build == 49284  ? 497
                                                : Build == 167409 ? 713
                                                                  : 735,
                              uint16_t remove = Build == 14505    ? 314
                                                : Build == 49284  ? 500
                                                : Build == 167409 ? 717
                                                                  : 739)
        : owner_(owner), create_(create), remove_(remove) {}
    struct Prepared {
        ConditionState<Build> state;
        std::vector<ConditionEntry> published;
        bool save = false, changed = false;
    };
    bool Prepare(const WorldActionContext& context, unsigned removeKinds, unsigned level, Prepared& next,
                 WorldActionOutput& out) const {
        if (!owner_ || !next.state.Load(context.conditionState))
            return false;
        if (!context.wallNow)
            return next.state.Entries().empty() && published_.empty();
        next.state.Expire(context.wallNow);
        next.state.RemoveKinds(removeKinds, context.wallNow);
        next.save = next.state.Text() != context.conditionState;
        for (const auto& entry : next.state.Entries())
            if (entry.startsUtc <= context.wallNow)
                next.published.push_back(entry);
        for (const auto& old : published_)
            if (std::find(next.published.begin(), next.published.end(), old) == next.published.end()) {
                if (!Remove(owner_, old, out.owned) || !Remove(context.peerAgent, old, out.peers))
                    return false;
                next.changed = true;
            }
        for (const auto& entry : next.published)
            if (std::find(published_.begin(), published_.end(), entry) == published_.end()) {
                if (!Create(owner_, entry, context.wallNow, level, out.owned) ||
                    !Create(context.peerAgent, entry, context.wallNow, level, out.peers))
                    return false;
                next.changed = true;
            }
        return true;
    }
    void Commit(Prepared&& next) {
        published_ = std::move(next.published);
    }
    bool Snapshot(uint16_t actor, uint64_t utc, unsigned level, Buffer& out) const {
        for (const auto& entry : published_)
            if (entry.endsUtc > utc && !Create(actor, entry, utc, level, out))
                return false;
        return true;
    }

  private:
    bool Create(uint16_t actor, const ConditionEntry& entry, uint64_t utc, unsigned level, Buffer& out) const {
        const uint64_t remaining = entry.endsUtc > utc ? entry.endsUtc - utc : 0;
        return actor && level <= 80 && remaining && remaining <= ConditionState<Build>::MaximumDuration &&
               out.U16(create_) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(entry.definition) &&
               out.VarU32(kFirstConditionInstance + entry.instance) && out.VarU32(static_cast<unsigned>(remaining)) &&
               out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    bool Remove(uint16_t actor, const ConditionEntry& entry, Buffer& out) const {
        return actor && out.U16(remove_) && out.VarU32(actor) && out.VarU32(actor) &&
               out.VarU32(kFirstConditionInstance + entry.instance);
    }
    std::vector<ConditionEntry> published_;
    uint16_t owner_, create_, remove_;
};
} // namespace modern
} // namespace glide
