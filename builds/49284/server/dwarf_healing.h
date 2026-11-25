#include "glide/profile/infuse_light.h"
#include "glide/profile/boon_removal.h"
#pragma once
#include "glide/profile/modern_health.h"
#include "glide/timed_self_effect.h"
#include "pain_absorption.h"
#include "glide/profile/inspiring_field.h"
#include "glide/profile/target_approach.h"
#include "glide/profile/backstep_data.h"
#include "glide/profile/demon_leap_data.h"
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/dragon_facet_data.h"
#include "glide/profile/regeneration.h"
#include "glide/profile/great_dwarf_allies.h"

namespace glide {
namespace hot49284 {

struct DwarfHealing {
    static const unsigned Skill = 27372, Legend = 13, Slot = 0, Recharge = 30000, Activation = 1333, Animation = 1853;
    static const unsigned Buff = 873, Duration = 2000, Removal = 3;
    static int Amount(unsigned level) {
        // Attribute6 is zero in this exploration profile; no invented gear stats.
        return 125 + static_cast<int>(static_cast<float>(level * level) * 0.7f + 0.5f);
    }
};
// 28219 records and metadata override370615 (same identity as Actual) back the bounded per-condition-kind
// adaptation. 742/4000ms targets its caster by the original demon-stance description (Roy Cronacher):
// https://www.guildwars2.com/en/news/revenant-the-master-of-the-mists/
// Calculate healing before adding 742; this is the local application order. 26980/trait1789 stays excluded; no weakness
// combat modifiers are simulated.
struct DemonHealing {
    static const unsigned Skill = 28219, Legend = 14, Slot = 0, Recharge = 30000, Activation = 1450;
    static const unsigned SelfCondition = 742, SelfConditionDuration = 4000;
    static int Base(unsigned level) {
        return 120 + static_cast<int>(static_cast<float>(level * level) * 0.5f + 0.5f);
    }
    static int PerCondition(unsigned level) {
        return 20 + static_cast<int>(static_cast<float>(level * level) * 0.15f + 0.5f);
    }
};
// 27975: base26596. The bounded allied delivery uses local radius600;
// 737/trait1795 and mitigation remain outside this state-publication adapter.
struct GreatDwarf {
    static const unsigned Skill = 27975, Legend = 13, Slot = 4, Activation = 3000, Animation = 3500, Buff = 26596,
                          Duration = 5000;
};
// Personal cycle only. Self torment recipient combines 19426/category2/fact with the retained Roy Cronacher
// article above. One application, never inferred periodic damage.
struct DemonElite {
    static const unsigned Skill = 28287, Stop = 26693, Legend = 14, Slot = 4, SelfCondition = 19426, Duration = 3000;
};
struct InspiringReinforcement {
    static const unsigned Skill = 28516, Legend = 13, Slot = 1, Activation = 501, Animation = 1233, Recharge = 5000;
};
struct AssassinApproach : modern::TargetApproach<49284> {
    static const unsigned Legend = 12, Slot = 1;
};
struct AssassinBackstep : modern::BackstepData<49284> {
    static const unsigned Legend = 12, Slot = 3;
};
struct DemonLeap : modern::DemonLeapData<49284> {
    static const unsigned Legend = 14, Slot = 3;
};
inline unsigned SupportLegend(unsigned skill) {
    if (skill == 26679)
        return 13;
    if (skill == 28406)
        return 12;
    if (skill == 27505)
        return 14;
    if (skill == 26937)
        return 12;
    if (modern::FacetData<49284>::IsFacetSkill(skill))
        return modern::FacetData<49284>::Legend;
    if (skill == DemonLeap::Skill)
        return DemonLeap::Legend;
    if (skill == AssassinBackstep::Skill)
        return AssassinBackstep::Legend;
    if (skill == AssassinApproach::Skill)
        return AssassinApproach::Legend;
    if (skill == InspiringReinforcement::Skill)
        return InspiringReinforcement::Legend;
    if (skill == PainAbsorptionAction::Rules::Skill)
        return PainAbsorptionAction::Rules::Legend();
    return skill == DemonHealing::Skill                                   ? DemonHealing::Legend
           : (skill == DwarfHealing::Skill || skill == GreatDwarf::Skill) ? DwarfHealing::Legend
                                                                          : 0;
}
inline bool IsSupportAction(unsigned skill) {
    return SupportLegend(skill) != 0;
}
// HoT-local form of the existing China multientry positive-effect controller.
// Accept the previous single873 entry and retain comma-separated v1 entries;
// H1 remains the outer energy envelope. Weapon stances stay in their controller.
class DwarfHealingEffects {
    struct Entry {
        uint32_t instance = 0, buff = 0;
        uint64_t startsUtc = 0, endsUtc = 0, endsAt = 0;
        bool announced = false;
    };
    static unsigned Duration(unsigned buff) {
        return buff == combat::InfuseLight::Buff                   ? combat::InfuseLight::Duration(49284)
               : buff == 873                                       ? 2000
               : (buff == 26596 || buff == AssassinApproach::Buff) ? 5000
               : buff == 26980                                     ? 3600000
                                                                   : modern::MaximumReceivedEffectDuration<49284>(buff);
    }

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

    bool Load(const Character& saved) {
        *this = DwarfHealingEffects();
        const auto found = saved.profileFields.find("skill_effects");
        if (found == saved.profileFields.end() || found->second.empty())
            return true;
        std::string text;
        if (!regeneration_.Load(found->second, text))
            return false;
        const auto bar = text.find('|');
        uint64_t counter = 0;
        if (text.size() > 8192 || text.compare(0, 2, "1:") || bar == std::string::npos ||
            !Decimal(text.substr(2, bar - 2), counter) || !counter || counter > modern::kFirstConditionInstance)
            return false;
        next_ = static_cast<uint32_t>(counter);
        size_t cursor = bar + 1;
        while (cursor < text.size()) {
            const auto comma = text.find(',', cursor), stop = comma == std::string::npos ? text.size() : comma;
            uint64_t f[4]{};
            for (unsigned i = 0; i < 4; ++i) {
                const auto end = i == 3 ? stop : text.find(':', cursor);
                if (end == std::string::npos || end > stop || !Decimal(text.substr(cursor, end - cursor), f[i]))
                    return false;
                cursor = end + 1;
            }
            if (entries_.size() >= 64 || !f[0] || f[0] >= next_ || f[0] >= modern::kFirstConditionInstance ||
                f[1] > UINT32_MAX || !Duration(static_cast<unsigned>(f[1])) || !f[2] || f[3] <= f[2] ||
                ((f[1] == 26980 || modern::MaximumReceivedEffectDuration<49284>(static_cast<unsigned>(f[1])))
                     ? f[3] - f[2] > Duration(static_cast<unsigned>(f[1]))
                     : f[3] - f[2] != Duration(static_cast<unsigned>(f[1]))))
                return false;
            for (const auto& entry : entries_)
                if (entry.instance == f[0])
                    return false;
            Entry e;
            e.instance = static_cast<uint32_t>(f[0]);
            e.buff = static_cast<uint32_t>(f[1]);
            e.startsUtc = f[2];
            e.endsUtc = f[3];
            entries_.push_back(e);
            if (comma == std::string::npos)
                break;
            if (cursor == text.size())
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
    bool PrepareRegeneration(uint64_t utc, float& health, float maximum, float& delta, bool& changed) {
        return regeneration_.Prepare(LegacyText(), utc, health, maximum, delta, changed);
    }
    bool Reserve(uint32_t& instance) {
        if (!next_ || next_ >= modern::kFirstConditionInstance)
            return false;
        instance = next_++;
        return true;
    }
    bool Restore(uint64_t now, uint64_t utc) {
        if (entries_.empty())
            return true;
        if (!utc)
            return false;
        for (auto& e : entries_) {
            if (e.endsUtc <= utc)
                continue;
            const auto remaining = e.endsUtc - utc;
            if (remaining > Duration(e.buff) || now > UINT64_MAX - remaining)
                return false;
            e.endsAt = now + remaining;
        }
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.endsUtc <= utc; }),
            entries_.end());
        return true;
    }
    bool Start(uint16_t actor, const WorldActionContext& c, uint64_t activation, unsigned level, Buffer& out,
               unsigned skill = DwarfHealing::Skill, uint64_t activationUtc = 0) {
        const unsigned buff = skill == combat::InfuseLight::Skill ? combat::InfuseLight::Buff
                              : skill == DwarfHealing::Skill      ? 873
                              : skill == GreatDwarf::Skill        ? 26596
                              : skill == AssassinApproach::Skill  ? AssassinApproach::Buff
                                                                  : 0,
                       duration = Duration(buff);
        if (!duration || c.now < activation || !c.wallNow)
            return false;
        if (activationUtc && c.wallNow < activationUtc)
            return false;
        const auto elapsed = activationUtc ? c.wallNow - activationUtc : c.now - activation;
        if (elapsed >= duration)
            return true;
        if (c.wallNow <= elapsed || c.wallNow - elapsed > UINT64_MAX - duration ||
            c.now > UINT64_MAX - (duration - elapsed))
            return false;
        for (const auto& e : entries_)
            if (e.buff == buff && e.announced && !Remove(actor, e.instance, out))
                return false;
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.buff == buff; }),
                       entries_.end());
        Entry e;
        if (entries_.size() >= 64 || !Reserve(e.instance))
            return false;
        e.buff = buff;
        e.startsUtc = c.wallNow - elapsed;
        e.endsUtc = e.startsUtc + duration;
        e.endsAt = c.now + duration - elapsed;
        e.announced = true;
        if (!Create(e, actor, c.now, level, out))
            return false;
        entries_.push_back(e);
        return true;
    }
    bool GrantAbsorption(uint16_t actor, const WorldActionContext& c, unsigned level,
                         const modern::PainAbsorptionGrant& grant, Buffer& out) {
        if (grant.buff != 26980 || !grant.startsUtc || grant.endsUtc <= grant.startsUtc || !c.wallNow)
            return false;
        if (grant.endsUtc <= c.wallNow)
            return true;
        uint64_t remaining = grant.endsUtc - c.wallNow;
        // Explicit local duration-stacking policy. Keep the unelapsed part of
        // the previous resistance and the new grant; late Tick adds no elapsed time.
        for (const auto& e : entries_)
            if (e.buff == grant.buff && e.endsUtc > c.wallNow)
                remaining += e.endsUtc - c.wallNow;
        if (remaining > Duration(grant.buff) || c.now > UINT64_MAX - remaining || c.wallNow > UINT64_MAX - remaining)
            return false;
        for (const auto& e : entries_)
            if (e.buff == grant.buff && e.announced && !Remove(actor, e.instance, out))
                return false;
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.buff == grant.buff; }),
            entries_.end());
        Entry e;
        if (entries_.size() >= 64 || !Reserve(e.instance))
            return false;
        e.buff = grant.buff;
        e.startsUtc = c.wallNow;
        e.endsUtc = c.wallNow + remaining;
        e.endsAt = c.now + remaining;
        e.announced = true;
        if (!Create(e, actor, c.now, level, out))
            return false;
        entries_.push_back(e);
        return true;
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
        } else if (entries_.size() >= 64 || next_ >= modern::kFirstConditionInstance)
            return false;
        Entry e;
        if (request.refreshInstance)
            e.instance = request.refreshInstance;
        else if (!Reserve(e.instance))
            return false;
        e.buff = request.definition;
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

  private:
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
        return actor && level <= 80 && now < e.endsAt && out.U16(497) && out.VarU32(actor) && out.VarU32(actor) &&
               out.VarU32(e.buff) && out.VarU32(e.instance) && out.VarU32(static_cast<uint32_t>(e.endsAt - now)) &&
               out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    uint32_t next_ = 1;
    std::vector<Entry> entries_;
    modern::Regeneration regeneration_;
};
} // namespace hot49284
} // namespace glide
