#include "glide/profile/boon_removal.h"
#pragma once
#include "shocking_aura_data.h"
#include "stance_rules.h"
#include "thief_healing.h"
#include "engineer_healing.h"
#include "glide/timed_self_effect.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/regeneration.h"
#include <algorithm>
#include <vector>

namespace glide {
namespace hot49284 {
struct SelfEffectRules {
    static const unsigned kCreate = 497, kRemove = 500;
    static unsigned Duration(unsigned buff, unsigned profession) {
        if (profession == 5 && buff == ThiefHealing::Stealth)
            return ThiefHealing::StealthDuration;
        if (profession == 6 && buff == kAirAuraBuff)
            return kAirAuraDurationMs;
        for (const auto& r : kStanceRules)
            if (r.profession == profession && r.buff == buff)
                return r.durationMs;
        return 0;
    }
    static unsigned SkillDuration(unsigned skill) {
        if (skill == ThiefHealing::Skill)
            return ThiefHealing::StealthDuration;
        const auto* stance = StanceForSkill(skill);
        return stance ? stance->durationMs : skill == kAirAuraSkill ? kAirAuraDurationMs : 0;
    }
    static unsigned SkillBuff(unsigned skill) {
        if (skill == ThiefHealing::Skill)
            return ThiefHealing::Stealth;
        const auto* stance = StanceForSkill(skill);
        return stance ? stance->buff : kAirAuraBuff;
    }
    static bool IsStance(unsigned buff) {
        return buff != kAirAuraBuff && buff != ThiefHealing::Stealth;
    }
};
// Local v1 extension: one native aura/stance plus independently received1122.
// Shared TimedSelfEffect and other builds retain their original single-entry API.
class SelfEffect {
    struct Entry {
        uint32_t instance = 0, buff = 0, duration = 0;
        uint64_t startsUtc = 0, endsUtc = 0, endsAt = 0;
        bool announced = false;
    };

  public:
    void Boons(std::vector<combat::LiveBoon>& out) const {
        for (const auto& e : entries_) {
            combat::LiveBoon row;
            row.instance = e.instance;
            row.definition = e.buff;
            row.starts = e.startsUtc;
            row.ends = e.endsUtc;
            row.announced = e.announced;
            out.push_back(row);
        }
    }
    void ForgetBoons(const std::set<unsigned>& ids) {
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) { return ids.count(e.instance) != 0; }),
                       entries_.end());
        regeneration_.Forget(ids);
    }

    bool Load(const Character& saved, unsigned profession) {
        *this = SelfEffect();
        const auto found = saved.profileFields.find("skill_effects");
        if (found == saved.profileFields.end() || found->second.empty())
            return true;
        std::string text;
        if (!regeneration_.Load(found->second, text))
            return false;
        const auto bar = text.find('|');
        uint64_t counter = 0;
        if (text.size() > 8192 || text.compare(0, 2, "1:") || bar == std::string::npos ||
            !Decimal(text.substr(2, bar - 2), counter) || !counter || counter > UINT32_MAX)
            return false;
        next_ = static_cast<uint32_t>(counter);
        size_t at = bar + 1;
        bool native = false;
        while (at < text.size()) {
            const auto comma = text.find(',', at), stop = comma == std::string::npos ? text.size() : comma;
            uint64_t f[4]{};
            for (unsigned i = 0; i < 4; ++i) {
                const auto end = i == 3 ? stop : text.find(':', at);
                if (end == std::string::npos || end > stop || !Decimal(text.substr(at, end - at), f[i]))
                    return false;
                at = end + 1;
            }
            if (entries_.size() >= 64 || !f[0] || f[0] >= next_ || f[0] >= UINT32_MAX - 6 || f[1] > UINT32_MAX ||
                !f[2] || f[3] <= f[2])
                return false;
            const unsigned buff = static_cast<unsigned>(f[1]),
                           personal = profession == 3                  ? EngineerHealing::Duration(buff)
                                      : profession == 5 && buff == 718 ? ThiefHealing::RegenerationDuration
                                                                       : 0;
            const unsigned duration = personal                 ? personal
                                      : ReceivedDuration(buff) ? ReceivedDuration(buff)
                                                               : SelfEffectRules::Duration(buff, profession);
            if (!duration || (Independent(buff) ? f[3] - f[2] > duration : f[3] - f[2] != duration) ||
                (!Independent(buff) && native))
                return false;
            for (const auto& e : entries_)
                if (e.instance == f[0])
                    return false;
            Entry e;
            e.instance = static_cast<uint32_t>(f[0]);
            e.buff = buff;
            e.duration = duration;
            e.startsUtc = f[2];
            e.endsUtc = f[3];
            entries_.push_back(e);
            if (!Independent(buff))
                native = true;
            if (comma == std::string::npos)
                break;
            if (at == text.size())
                return false;
        }
        return true;
    }
    std::string LegacyText() const {
        auto text = "1:" + std::to_string(next_) + "|";
        for (const auto& e : entries_) {
            if (text.back() != '|')
                text += ',';
            text += std::to_string(e.instance) + ':' + std::to_string(e.buff) + ':' + std::to_string(e.startsUtc) +
                    ':' + std::to_string(e.endsUtc);
        }
        return text;
    }
    std::string Text() const {
        return regeneration_.Text(LegacyText());
    }
    bool RegenerationEnabled() const {
        return regeneration_.Enabled();
    }
    bool PrepareRegeneration(uint64_t utc, float& health, float maximum, float& delta, bool& changed) {
        return regeneration_.Prepare(LegacyText(), utc, health, maximum, delta, changed);
    }
    // Only received contributions require a new durable expiry write. Native
    // aura/stance restoration and expiry keep their pre-existing publication
    // contract; announcing an unchanged entry never constitutes a state save.
    std::string ReceivedText() const {
        std::string text;
        for (const auto& e : entries_)
            if (Independent(e.buff) || e.buff == ThiefHealing::Stealth) {
                if (!text.empty())
                    text += ',';
                text +=
                    std::to_string(e.instance) + ':' + std::to_string(e.startsUtc) + ':' + std::to_string(e.endsUtc);
            }
        return text;
    }
    bool Restore(uint64_t now, uint64_t utc) {
        if (!entries_.empty() && !utc)
            return false;
        for (auto& e : entries_)
            if (e.endsUtc > utc) {
                const auto left = e.endsUtc - utc;
                if (left > e.duration || now > UINT64_MAX - left)
                    return false;
                e.endsAt = now + left;
            }
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.endsUtc <= utc; }),
            entries_.end());
        return true;
    }
    bool Start(uint16_t actor, unsigned skill, uint64_t now, uint64_t utc, unsigned level, Buffer& out,
               uint64_t elapsed = 0) {
        const auto duration = SelfEffectRules::SkillDuration(skill);
        if (!duration || !utc || elapsed >= duration || elapsed > now || elapsed >= utc ||
            now > UINT64_MAX - duration || utc > UINT64_MAX - duration || next_ >= UINT32_MAX - 6)
            return false;
        bool native = false;
        for (const auto& e : entries_)
            if (!Independent(e.buff))
                native = true;
        if (!native && entries_.size() >= 64)
            return false;
        for (const auto& e : entries_)
            if (!Independent(e.buff) && e.announced && !Remove(actor, e.instance, out))
                return false;
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(), [](const Entry& e) { return !Independent(e.buff); }),
            entries_.end());
        Entry e;
        e.instance = next_++;
        e.buff = SelfEffectRules::SkillBuff(skill);
        e.duration = duration;
        e.startsUtc = utc - elapsed;
        e.endsUtc = e.startsUtc + duration;
        e.endsAt = now + duration - elapsed;
        e.announced = true;
        if (!Create(e, actor, now, level, out))
            return false;
        entries_.push_back(e);
        return true;
    }
    unsigned ActiveBuff(uint64_t now) const {
        for (const auto& e : entries_)
            if (!Independent(e.buff) && now < e.endsAt)
                return e.buff;
        return 0;
    }
    bool SuppressStealth(uint16_t actor, Buffer& out, bool& changed) {
        changed = false;
        for (const auto& e : entries_)
            if (e.buff == ThiefHealing::Stealth) {
                if (e.announced && !Remove(actor, e.instance, out))
                    return false;
                changed = true;
            }
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [](const Entry& e) { return e.buff == ThiefHealing::Stealth; }),
                       entries_.end());
        return true;
    }
    bool GrantThiefHealing(uint16_t actor, const WorldActionContext& c, uint64_t startsUtc, unsigned level,
                           Buffer& out) {
        if (!startsUtc || c.wallNow < startsUtc)
            return false;
        const auto elapsed = c.wallNow - startsUtc;
        if (!c.revealed && elapsed < ThiefHealing::StealthDuration &&
            !Start(actor, ThiefHealing::Skill, c.now, c.wallNow, level, out, elapsed))
            return false;
        if (elapsed >= ThiefHealing::RegenerationDuration)
            return true;
        const auto left = ThiefHealing::RegenerationDuration - elapsed;
        if (entries_.size() >= 64 || next_ >= UINT32_MAX - 6 || c.now > UINT64_MAX - left ||
            startsUtc > UINT64_MAX - ThiefHealing::RegenerationDuration)
            return false;
        Entry e;
        e.instance = next_++;
        e.buff = 718;
        e.duration = ThiefHealing::RegenerationDuration;
        e.startsUtc = startsUtc;
        e.endsUtc = startsUtc + e.duration;
        e.endsAt = c.now + left;
        e.announced = true;
        if (!regeneration_.Grant(e.instance, level, c.wallNow, e.endsUtc) || !Create(e, actor, c.now, level, out))
            return false;
        entries_.push_back(e);
        return true;
    }
    bool GrantEngineerHealing(uint16_t actor, const WorldActionContext& c, uint64_t startsUtc, unsigned level,
                              Buffer& out) {
        if (!startsUtc || c.wallNow < startsUtc)
            return false;
        const auto elapsed = c.wallNow - startsUtc;
        for (unsigned buff : {717u, 718u, 719u}) {
            const auto duration = EngineerHealing::Duration(buff);
            if (elapsed >= duration)
                continue;
            const auto left = duration - elapsed;
            if (entries_.size() >= 64 || next_ >= UINT32_MAX - 6 || c.now > UINT64_MAX - left ||
                startsUtc > UINT64_MAX - duration)
                return false;
            Entry e;
            e.instance = next_++;
            e.buff = buff;
            e.duration = duration;
            e.startsUtc = startsUtc;
            e.endsUtc = startsUtc + duration;
            e.endsAt = c.now + left;
            e.announced = true;
            if ((buff == 718 && !regeneration_.Grant(e.instance, level, c.wallNow, e.endsUtc)) ||
                !Create(e, actor, c.now, level, out))
                return false;
            entries_.push_back(e);
        }
        return true;
    }
    bool ActiveStance(uint64_t now) const {
        const auto buff = ActiveBuff(now);
        return buff && SelfEffectRules::IsStance(buff);
    }
    bool Consume(uint16_t actor, Buffer& out) {
        for (auto it = entries_.begin(); it != entries_.end(); ++it)
            if (!Independent(it->buff)) {
                if (it->announced && !Remove(actor, it->instance, out))
                    return false;
                entries_.erase(it);
                return true;
            }
        return false;
    }
    bool Receive(uint16_t actor, const WorldActionContext& c, const ReceivedWorldEffect& incoming, unsigned level,
                 unsigned& instance, Buffer& out) {
        auto request = incoming;
        if (combat::BoonRemovals::Removed(c.removedBoons, request.refreshInstance, request.definition, c.wallNow))
            request.refreshInstance = 0;
        if (!c.wallNow || request.endsUtc <= c.wallNow ||
            !modern::AllowedReceivedEffect<49284>(request.definition, request.endsUtc - c.wallNow) ||
            c.now > UINT64_MAX - (request.endsUtc - c.wallNow))
            return false;
        auto at = entries_.end();
        if (request.refreshInstance) {
            at = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& e) {
                return e.instance == request.refreshInstance && e.buff == request.definition && e.endsUtc > c.wallNow;
            });
            if (at == entries_.end())
                return false;
        } else if (entries_.size() >= 64 || next_ >= UINT32_MAX - 6)
            return false;
        Entry e;
        e.instance = request.refreshInstance ? request.refreshInstance : next_++;
        e.buff = request.definition;
        e.duration = ReceivedDuration(e.buff);
        e.startsUtc = c.wallNow;
        e.endsUtc = request.endsUtc;
        e.endsAt = c.now + request.endsUtc - c.wallNow;
        e.announced = true;
        if (at != entries_.end() && at->announced && !Remove(actor, at->instance, out))
            return false;
        if (e.buff == 718 && !regeneration_.Grant(e.instance, request.sourceLevel, c.wallNow, request.endsUtc))
            return false;
        if (!Create(e, actor, c.now, level, out))
            return false;
        if (at != entries_.end())
            *at = e;
        else
            entries_.push_back(e);
        instance = e.instance;
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, unsigned level, Buffer& out) const {
        for (const auto& e : entries_)
            if (e.announced && now < e.endsAt && !Create(e, actor, now, level, out))
                return false;
        return true;
    }
    bool Cleanup(uint16_t actor, Buffer& out) const {
        for (const auto& e : entries_)
            if (e.announced && !Remove(actor, e.instance, out))
                return false;
        return true;
    }
    bool Advance(uint16_t actor, uint64_t now, unsigned level, Buffer& out, bool& changed) {
        changed = false;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (now >= it->endsAt) {
                if (it->announced && !Remove(actor, it->instance, out))
                    return false;
                it = entries_.erase(it);
                changed = true;
                continue;
            }
            if (!it->announced) {
                if (!Create(*it, actor, now, level, out))
                    return false;
                it->announced = true;
                changed = true;
            }
            ++it;
        }
        return true;
    }

  private:
    static bool Independent(unsigned buff) {
        return buff == 717 || ReceivedDuration(buff) != 0;
    }
    static unsigned ReceivedDuration(unsigned buff) {
        return modern::MaximumReceivedEffectDuration<49284>(buff);
    }
    static bool Decimal(const std::string& text, uint64_t& value) {
        if (text.empty() || (text.size() > 1 && text[0] == '0'))
            return false;
        value = 0;
        for (char c : text) {
            if (c < '0' || c > '9' || value > (UINT64_MAX - (c - '0')) / 10)
                return false;
            value = value * 10 + c - '0';
        }
        return true;
    }
    static bool Remove(uint16_t actor, uint32_t instance, Buffer& out) {
        return actor && instance && out.U16(500) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(instance);
    }
    static bool Create(const Entry& e, uint16_t actor, uint64_t now, unsigned level, Buffer& out) {
        return actor && level <= UINT8_MAX && now < e.endsAt && e.endsAt - now <= e.duration && out.U16(497) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(e.buff) && out.VarU32(e.instance) &&
               out.VarU32(static_cast<uint32_t>(e.endsAt - now)) && out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    uint32_t next_ = 1;
    std::vector<Entry> entries_;
    modern::Regeneration regeneration_;
};
} // namespace hot49284
} // namespace glide
