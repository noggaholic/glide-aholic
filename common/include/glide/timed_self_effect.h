#pragma once
#include "glide/character.h"
#include "glide/buffer.h"
#include <string>

namespace glide {
// Same durable version1/UTC format and atomic skill-state save as Build 204489.
// A single timed effect, with each build supplying definitions and wire IDs.
template <class Rules, uint32_t FirstReserved = UINT32_MAX> class TimedSelfEffect {
  public:
    explicit TimedSelfEffect(uint16_t create = Rules::kCreate, uint16_t remove = Rules::kRemove)
        : create_(create), remove_(remove) {}
    bool Load(const Character& saved, unsigned profession) {
        *this = TimedSelfEffect(create_, remove_);
        const auto found = saved.profileFields.find("skill_effects");
        if (found == saved.profileFields.end() || found->second.empty())
            return true;
        const auto& text = found->second;
        const auto bar = text.find('|');
        uint64_t next = 0;
        if (text.size() > 160 || text.compare(0, 2, "1:") || bar == std::string::npos ||
            !Decimal(text.substr(2, bar - 2), next) || !next || next > UINT32_MAX)
            return false;
        next_ = static_cast<uint32_t>(next);
        if (bar + 1 == text.size())
            return true;
        uint64_t fields[4]{};
        size_t begin = bar + 1;
        for (unsigned i = 0; i < 4; ++i) {
            const auto end = i == 3 ? text.size() : text.find(':', begin);
            if (end == std::string::npos || !Decimal(text.substr(begin, end - begin), fields[i]))
                return false;
            begin = end + 1;
        }
        const unsigned duration = Rules::Duration(static_cast<unsigned>(fields[1]), profession);
        if (!fields[0] || fields[0] >= next_ || fields[0] >= FirstReserved || fields[1] > UINT32_MAX || !duration ||
            !fields[2] || fields[3] <= fields[2] || fields[3] - fields[2] != duration)
            return false;
        buff_ = static_cast<unsigned>(fields[1]);
        duration_ = duration;
        instance_ = static_cast<uint32_t>(fields[0]);
        startsUtc_ = fields[2];
        endsUtc_ = fields[3];
        return true;
    }
    std::string Text() const {
        auto text = "1:" + std::to_string(next_) + "|";
        if (instance_)
            text += std::to_string(instance_) + ':' + std::to_string(buff_) + ':' + std::to_string(startsUtc_) + ':' +
                    std::to_string(endsUtc_);
        return text;
    }
    bool Restore(uint64_t now, uint64_t utc) {
        if (!instance_)
            return true;
        if (!utc)
            return false;
        if (endsUtc_ <= utc) {
            instance_ = 0;
            return true;
        }
        const auto remaining = endsUtc_ - utc;
        if (remaining > duration_ || now > UINT64_MAX - remaining)
            return false;
        endsAt_ = now + remaining;
        return true;
    }
    bool Start(uint16_t actor, unsigned skill, uint64_t now, uint64_t utc, unsigned level, Buffer& bytes,
               uint64_t elapsed = 0) {
        const unsigned duration = Rules::SkillDuration(skill);
        if (!duration || !utc || elapsed >= duration || elapsed > now || elapsed >= utc ||
            now > UINT64_MAX - duration || utc > UINT64_MAX - duration || next_ >= FirstReserved)
            return false;
        if (instance_ && announced_ && !Remove(actor, instance_, bytes))
            return false;
        buff_ = Rules::SkillBuff(skill);
        duration_ = duration;
        instance_ = next_++;
        startsUtc_ = utc - elapsed;
        endsUtc_ = startsUtc_ + duration;
        endsAt_ = now + duration - elapsed;
        if (!Create(actor, now, level, bytes))
            return false;
        announced_ = true;
        return true;
    }
    unsigned ActiveBuff(uint64_t now) const {
        return instance_ && now < endsAt_ ? buff_ : 0;
    }
    bool ActiveStance(uint64_t now) const {
        return ActiveBuff(now) && Rules::IsStance(buff_);
    }
    bool Consume(uint16_t actor, Buffer& bytes) {
        if (!instance_)
            return false;
        if (announced_ && !Remove(actor, instance_, bytes))
            return false;
        instance_ = 0;
        announced_ = false;
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes) const {
        return !instance_ || !announced_ || now >= endsAt_ || Create(actor, now, level, bytes);
    }
    bool Advance(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes, bool& changed) {
        changed = false;
        if (!instance_)
            return true;
        if (now >= endsAt_) {
            if (announced_ && !Remove(actor, instance_, bytes))
                return false;
            changed = announced_;
            instance_ = 0;
            announced_ = false;
            return true;
        }
        if (!announced_) {
            if (!Create(actor, now, level, bytes))
                return false;
            announced_ = true;
            changed = true;
        }
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
            value = value * 10 + (c - '0');
        }
        return true;
    }
    bool Create(uint16_t actor, uint64_t now, unsigned level, Buffer& out) const {
        const auto remaining = endsAt_ > now ? endsAt_ - now : 0;
        return actor && level <= UINT8_MAX && remaining && remaining <= duration_ && out.U16(create_) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(buff_) && out.VarU32(instance_) &&
               out.VarU32(static_cast<uint32_t>(remaining)) && out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    bool Remove(uint16_t actor, uint32_t instance, Buffer& out) const {
        return actor && instance && out.U16(remove_) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(instance);
    }
    uint16_t create_, remove_;
    uint32_t next_ = 1, instance_ = 0, buff_ = 0, duration_ = 0;
    uint64_t startsUtc_ = 0, endsUtc_ = 0, endsAt_ = 0;
    bool announced_ = false;
};
} // namespace glide
