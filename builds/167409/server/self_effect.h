#include "glide/profile/infuse_light.h"
#include "glide/profile/boon_removal.h"
#pragma once
#include "glide/profile/allied_effect_data.h"
#include "glide/profile/backstep_data.h"
#include "air_aura_data.h"
#include "stance_rules.h"
#include "glide/character.h"
#include "glide/buffer.h"
#include "glide/profile/modern_great_dwarf_data.h"
#include <algorithm>
#include <string>
#include <vector>
#include "glide/profile/modern_dwarf_effect.h"
#include "glide/profile/modern_condition_data.h"
#include "glide/profile/pain_absorption.h"
#include "glide/profile/target_approach.h"
#include "glide/profile/burst_strength_data.h"
#include "glide/profile/dragon_facet_data.h"
#include "glide/profile/regeneration.h"
#include "glide/profile/modern_health.h"

namespace glide {
namespace china167409 {
struct SelfEffectRules {

    static const unsigned kCreate = 713, kRemove = 717;
    static unsigned Duration(unsigned buff, unsigned profession) {
        if (profession == 9 && buff == modern::BackstepData<167409>::Boon())
            return modern::BackstepData<167409>::BoonDuration();
        if (profession == 9 && buff == combat::InfuseLight::Buff)
            return combat::InfuseLight::Duration(167409);
        if (profession == 9 && buff == modern::BurstStrength::Buff)
            return modern::BurstStrength::Duration;
        if (profession == 9 && buff == modern::TargetApproach<167409>::Buff)
            return modern::TargetApproach<167409>::Duration;
        if (profession == 3 && buff == 719)
            return 4000; // 5834 base, not the allied facet duration.
        if (profession == 5 && buff == 718)
            return 6000; // 13027 default fact, not the allied facet duration.
        if (const auto received = modern::MaximumReceivedEffectDuration<167409>(buff))
            return received; // Received boons are independent of profession.
        // Technical local persistence ceiling, not an original boon cap.
        if (profession == 9 && buff == 26980)
            return modern::ConditionState<167409>::MaximumDuration;
        if (profession == 9 && buff == modern::GreatDwarfData<167409>::Buff)
            return modern::GreatDwarfData<167409>::DurationMs;
        if (profession == 9 && buff == modern::DwarfEffectRules::Buff)
            return modern::DwarfEffectRules::DurationMs;
        if (profession == kAquaticStance.profession && buff == kAquaticStance.buff)
            return kAquaticStance.durationMs;
        return profession == 6 && buff == kAirAuraBuff ? kAirAuraDurationMs : 0;
    }
    static unsigned SkillDuration(unsigned skill) {
        if (skill == modern::TargetApproach<167409>::Skill)
            return modern::TargetApproach<167409>::Duration;
        if (skill == modern::GreatDwarfData<167409>::Skill)
            return modern::GreatDwarfData<167409>::DurationMs;
        if (skill == modern::DwarfEffectRules::Skill)
            return modern::DwarfEffectRules::DurationMs;
        const auto stance = StanceForSkill(skill);
        return stance ? stance->durationMs : skill == kAirAuraSkill ? kAirAuraDurationMs : 0;
    }
    static unsigned SkillBuff(unsigned skill) {
        if (skill == modern::TargetApproach<167409>::Skill)
            return modern::TargetApproach<167409>::Buff;
        if (skill == modern::GreatDwarfData<167409>::Skill)
            return modern::GreatDwarfData<167409>::Buff;
        if (skill == modern::DwarfEffectRules::Skill)
            return modern::DwarfEffectRules::Buff;
        const auto stance = StanceForSkill(skill);
        return stance ? stance->buff : kAirAuraBuff;
    }
    static bool IsStance(unsigned buff) {
        return buff == kAquaticStance.buff;
    }
};
// Local multientry version1 format, compatible with the prior single entry and
// Build 204489's comma-separated entries. Generic TimedSelfEffect remains unchanged.
class SelfEffect {
    struct Entry {
        uint32_t instance = 0, buff = 0, duration = 0;
        uint64_t startsUtc = 0, endsUtc = 0, startsAt = 0, endsAt = 0;
        bool announced = false;
    };
    std::vector<Entry> entries_;
    uint32_t next_ = 1;
    modern::Regeneration regeneration_;
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
    static bool Create(const Entry& e, uint16_t actor, uint64_t now, unsigned level, Buffer& out) {
        const auto remaining = e.endsAt > now ? e.endsAt - now : 0;
        return actor && level <= UINT8_MAX && now >= e.startsAt && remaining && remaining <= e.duration &&
               out.U16(SelfEffectRules::kCreate) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(e.buff) &&
               out.VarU32(e.instance) && out.VarU32(static_cast<uint32_t>(remaining)) &&
               out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    static bool Remove(uint16_t actor, uint32_t instance, Buffer& out) {
        return actor && instance && out.U16(SelfEffectRules::kRemove) && out.VarU32(actor) && out.VarU32(actor) &&
               out.VarU32(instance);
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

    bool ReserveInstance(uint32_t& instance) {
        if (next_ >= modern::kFirstConditionInstance)
            return false;
        instance = next_++;
        return true;
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
        uint64_t next = 0;
        if (text.size() > 8192 || text.compare(0, 2, "1:") || bar == std::string::npos ||
            !Decimal(text.substr(2, bar - 2), next) || !next || next > UINT32_MAX)
            return false;
        next_ = static_cast<uint32_t>(next);
        if (bar + 1 == text.size())
            return true;
        size_t cursor = bar + 1;
        while (cursor < text.size()) {
            const auto comma = text.find(',', cursor), stop = comma == std::string::npos ? text.size() : comma;
            uint64_t fields[4]{};
            for (unsigned i = 0; i < 4; ++i) {
                const auto end = i == 3 ? stop : text.find(':', cursor);
                if (end == std::string::npos || end > stop || !Decimal(text.substr(cursor, end - cursor), fields[i]))
                    return false;
                cursor = end + 1;
            }
            const unsigned duration = SelfEffectRules::Duration(static_cast<unsigned>(fields[1]), profession);
            if (entries_.size() >= 64 || !fields[0] || fields[0] >= next_ ||
                fields[0] >= modern::kFirstConditionInstance || fields[1] > UINT32_MAX || !duration || !fields[2] ||
                fields[3] <= fields[2] ||
                (fields[1] == 26980 || modern::MaximumReceivedEffectDuration<167409>(static_cast<unsigned>(fields[1]))
                     ? fields[3] - fields[2] > duration
                     : fields[3] - fields[2] != duration))
                return false;
            for (const auto& e : entries_)
                if (e.instance == fields[0])
                    return false;
            Entry e;
            e.instance = static_cast<uint32_t>(fields[0]);
            e.buff = static_cast<uint32_t>(fields[1]);
            e.duration = duration;
            e.startsUtc = fields[2];
            e.endsUtc = fields[3];
            e.duration = static_cast<unsigned>(fields[3] - fields[2]);
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
    bool PrepareRegeneration(uint64_t utc, float& health, float& delta, bool& changed) {
        return regeneration_.Prepare(LegacyText(), utc, health, modern::kPublishedMaximumHealth, delta, changed);
    }
    bool Restore(uint64_t now, uint64_t utc) {
        if (entries_.empty())
            return true;
        if (!utc)
            return false;
        for (auto& e : entries_) {
            if (e.endsUtc <= utc)
                continue;
            const auto delay = e.startsUtc > utc ? e.startsUtc - utc : 0, remaining = e.endsUtc - utc;
            const unsigned maximumDelay =
                e.buff == modern::BurstStrength::Buff
                    ? modern::FacetData<167409>::RuleFor(modern::BurstStrength::Skill).activation
                : e.buff == modern::GreatDwarfData<167409>::Buff ? modern::GreatDwarfData<167409>::ActivationMs
                                                                 : 0;
            if (delay > maximumDelay || remaining > e.duration + delay || now > UINT64_MAX - remaining)
                return false;
            e.startsAt = now + delay;
            e.endsAt = now + remaining;
        }
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.endsUtc <= utc; }),
            entries_.end());
        return true;
    }
    bool Schedule(unsigned skill, uint64_t now, uint64_t utc, bool water = false) {
        const bool light = skill == combat::InfuseLight::Skill;
        const bool burst = skill == modern::BurstStrength::Skill;
        if ((!light && !burst && skill != modern::GreatDwarfData<167409>::Skill) || entries_.size() >= 64 ||
            next_ >= modern::kFirstConditionInstance || !utc)
            return false;
        const uint64_t delay = light   ? 0
                               : burst ? modern::FacetData<167409>::RuleFor(skill, water).activation
                                       : modern::GreatDwarfData<167409>::ActivationMs;
        const uint64_t duration = light   ? combat::InfuseLight::Duration(167409)
                                  : burst ? modern::BurstStrength::Duration
                                          : SelfEffectRules::SkillDuration(skill);
        if (now > UINT64_MAX - delay - duration || utc > UINT64_MAX - delay - duration)
            return false;
        Entry e;
        e.instance = next_++;
        e.buff = light   ? combat::InfuseLight::Buff
                 : burst ? modern::BurstStrength::Buff
                         : SelfEffectRules::SkillBuff(skill);
        e.duration = static_cast<uint32_t>(duration);
        e.startsUtc = utc + delay;
        e.endsUtc = e.startsUtc + duration;
        e.startsAt = now + delay;
        e.endsAt = e.startsAt + duration;
        entries_.push_back(e);
        return true;
    }
    bool CancelPending(uint64_t now, unsigned definition = 0) {
        const auto before = entries_.size();
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                      [&](const Entry& e) {
                                          return !e.announced && (!definition || e.buff == definition) &&
                                                 (now < e.startsAt || definition);
                                      }),
                       entries_.end());
        return before != entries_.size();
    }
    bool GrantAbsorption(uint16_t actor, const modern::PainAbsorptionGrant& grant, const WorldActionContext& c,
                         unsigned level, Buffer& bytes) {
        if ((grant.buff != 26980 && grant.buff != 873) || !grant.startsUtc || grant.startsUtc > c.wallNow ||
            grant.endsUtc <= c.wallNow || grant.endsUtc <= grant.startsUtc || next_ >= modern::kFirstConditionInstance)
            return false;
        uint64_t remaining = grant.endsUtc - c.wallNow;
        bool merged = false;
        if (grant.buff == 26980)
            for (const auto& e : entries_)
                if (e.buff == 26980 && e.endsAt > c.now) {
                    remaining += e.endsAt - c.now;
                    merged = true;
                }
        const auto limit = modern::ConditionState<167409>::MaximumDuration;
        if (remaining > limit || c.now > UINT64_MAX - remaining || c.wallNow > UINT64_MAX - remaining)
            return false;
        if (grant.buff == 873 && grant.endsUtc - grant.startsUtc != 5000)
            return false;
        if (grant.buff == 26980) {
            for (const auto& e : entries_)
                if (e.buff == 26980 && e.announced && !Remove(actor, e.instance, bytes))
                    return false;
            entries_.erase(
                std::remove_if(entries_.begin(), entries_.end(), [](const Entry& e) { return e.buff == 26980; }),
                entries_.end());
        }
        if (entries_.size() >= 64)
            return false;
        Entry e;
        e.instance = next_++;
        e.buff = grant.buff;
        e.startsUtc = merged ? c.wallNow : grant.startsUtc;
        e.endsUtc = c.wallNow + remaining;
        e.duration = static_cast<unsigned>(e.endsUtc - e.startsUtc);
        const auto elapsed = c.wallNow - e.startsUtc;
        e.startsAt = c.now >= elapsed ? c.now - elapsed : 0;
        e.endsAt = c.now + remaining;
        if (!Create(e, actor, c.now, level, bytes))
            return false;
        e.announced = true;
        entries_.push_back(e);
        return true;
    }
    bool ReceiveProtection(uint16_t actor, const ReceivedWorldEffect& incoming, const WorldActionContext& c,
                           unsigned level, unsigned& instance, Buffer& bytes) {
        auto request = incoming;
        if (combat::BoonRemovals::Removed(c.removedBoons, request.refreshInstance, request.definition, c.wallNow))
            request.refreshInstance = 0;
        if (!c.wallNow || request.endsUtc <= c.wallNow ||
            !modern::AllowedReceivedEffect<167409>(request.definition, request.endsUtc - c.wallNow) ||
            c.now > UINT64_MAX - (request.endsUtc - c.wallNow))
            return false;
        auto found = entries_.end();
        if (request.refreshInstance) {
            found = std::find_if(entries_.begin(), entries_.end(),
                                 [&](const Entry& e) { return e.instance == request.refreshInstance; });
            if (found == entries_.end() || found->buff != request.definition || found->startsUtc > c.wallNow ||
                found->endsUtc <= c.wallNow)
                return false;
        } else if (entries_.size() >= 64 || next_ >= modern::kFirstConditionInstance)
            return false;
        Entry entry;
        entry.instance = request.refreshInstance ? request.refreshInstance : next_;
        entry.buff = request.definition;
        entry.duration = static_cast<unsigned>(request.endsUtc - c.wallNow);
        entry.startsUtc = c.wallNow;
        entry.endsUtc = request.endsUtc;
        entry.startsAt = c.now;
        entry.endsAt = c.now + entry.duration;
        entry.announced = true;
        if (entry.buff == 718 && !regeneration_.Grant(entry.instance, request.sourceLevel, c.wallNow, request.endsUtc))
            return false;
        Buffer output;
        if (found != entries_.end() && found->announced && !Remove(actor, found->instance, output))
            return false;
        if (!Create(entry, actor, c.now, level, output) || !bytes.Append(output.View()))
            return false;
        if (found != entries_.end())
            *found = entry;
        else {
            entries_.push_back(entry);
            ++next_;
        }
        instance = entry.instance;
        return true;
    }
    bool StartPersonalBenefit(uint16_t actor, unsigned skill, unsigned buff, uint64_t now, uint64_t utc, unsigned level,
                              Buffer& bytes, uint64_t elapsed) {
        const unsigned duration =
            skill == modern::BackstepData<167409>::Skill && buff == modern::BackstepData<167409>::Boon()
                ? modern::BackstepData<167409>::BoonDuration()
            : skill == 13027 && buff == 718 ? 6000
            : skill == 5834                 ? (buff == 717                    ? 2000
                                               : (buff == 718 || buff == 719) ? 4000
                                                                              : 0)
                                            : 0;
        if (!duration || elapsed >= duration || elapsed > now || elapsed >= utc || entries_.size() >= 64 ||
            next_ >= modern::kFirstConditionInstance || now > UINT64_MAX - duration || utc > UINT64_MAX - duration)
            return false;
        Entry e;
        e.instance = next_++;
        e.buff = buff;
        e.duration = duration;
        e.startsAt = now - elapsed;
        e.endsAt = e.startsAt + duration;
        e.startsUtc = utc - elapsed;
        e.endsUtc = e.startsUtc + duration;
        if ((buff == 718 && !regeneration_.Grant(e.instance, level, e.startsUtc, e.endsUtc)) ||
            !Create(e, actor, now, level, bytes))
            return false;
        e.announced = true;
        entries_.push_back(e);
        return true;
    }
    bool Start(uint16_t actor, unsigned skill, uint64_t now, uint64_t utc, unsigned level, Buffer& bytes,
               uint64_t elapsed = 0) {
        const unsigned duration = SelfEffectRules::SkillDuration(skill), buff = SelfEffectRules::SkillBuff(skill);
        if (!duration || !utc || elapsed >= duration || elapsed > now || elapsed >= utc ||
            now > UINT64_MAX - duration || utc > UINT64_MAX - duration || next_ >= modern::kFirstConditionInstance)
            return false;
        // Preserve unrelated effects (notably the dwarf heal and the Rite).
        for (const auto& e : entries_)
            if (e.buff == buff && e.announced && !Remove(actor, e.instance, bytes))
                return false;
        entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const Entry& e) { return e.buff == buff; }),
                       entries_.end());
        if (entries_.size() >= 64)
            return false;
        Entry e;
        e.instance = next_++;
        e.buff = buff;
        e.duration = duration;
        e.startsUtc = utc - elapsed;
        e.endsUtc = e.startsUtc + duration;
        e.startsAt = now - elapsed;
        e.endsAt = e.startsAt + duration;
        if (!Create(e, actor, now, level, bytes))
            return false;
        e.announced = true;
        entries_.push_back(e);
        return true;
    }
    unsigned ActiveBuff(uint64_t now) const {
        for (const auto& e : entries_)
            if (!modern::MaximumReceivedEffectDuration<167409>(e.buff) && now >= e.startsAt && now < e.endsAt)
                return e.buff;
        return 0;
    }
    bool ActiveStance(uint64_t now) const {
        for (const auto& e : entries_)
            if (now >= e.startsAt && now < e.endsAt && SelfEffectRules::IsStance(e.buff))
                return true;
        return false;
    }
    bool Consume(uint16_t actor, Buffer& bytes) {
        if (entries_.empty())
            return false;
        for (const auto& e : entries_)
            if (!modern::MaximumReceivedEffectDuration<167409>(e.buff) && e.announced &&
                !Remove(actor, e.instance, bytes))
                return false;
        entries_.erase(
            std::remove_if(entries_.begin(), entries_.end(),
                           [](const Entry& e) { return !modern::MaximumReceivedEffectDuration<167409>(e.buff); }),
            entries_.end());
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes) const {
        for (const auto& e : entries_)
            if (e.announced && now >= e.startsAt && now < e.endsAt && !Create(e, actor, now, level, bytes))
                return false;
        return true;
    }
    bool Advance(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes, bool& changed, uint64_t heldRiteEnd = 0) {
        changed = false;
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (now >= it->endsAt) {
                if (it->announced && !Remove(actor, it->instance, bytes))
                    return false;
                changed |= it->announced;
                it = entries_.erase(it);
                continue;
            }
            if (!it->announced && now >= it->startsAt &&
                !(heldRiteEnd && it->buff == modern::GreatDwarfData<167409>::Buff && it->endsUtc == heldRiteEnd)) {
                if (!Create(*it, actor, now, level, bytes))
                    return false;
                it->announced = true;
                changed = true;
            }
            ++it;
        }
        return true;
    }
};
} // namespace china167409
} // namespace glide
