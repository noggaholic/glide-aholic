#pragma once
#include "glide/timed_self_effect.h"
#include "glide/profile/beta_symbol_data.h"
#include "glide/profile/beta_health.h"
#include "glide/profile/beta_personal_data.h"
#include "glide/profile/beta_protection_data.h"
#include "glide/profile/beta_rock_barrier_data.h"
#include "glide/profile/beta_phoenix_data.h"
#include "glide/profile/beta_siren_data.h"
#include "glide/profile/beta_positive_effect_state.h"
#include <algorithm>
#include <set>

namespace glide {
namespace beta {
// Preserve the existing stance text until the first personal/received boon. Afterwards
// a length-prefixed envelope keeps both ledgers, with disjoint instance IDs.
template <class Rules> class PositiveEffects {
    enum : uint32_t { FirstReceived = 0x40000000u };
    struct Boon {
        uint32_t id = 0, definition = 289;
        uint64_t start = 0, end = 0, endsAt = 0;
        bool announced = false;
    };
    TimedSelfEffect<Rules, FirstReceived> stance_;
    Regeneration regeneration_;
    std::vector<Boon> boons_;
    uint32_t next_ = FirstReceived;
    uint16_t create_, remove_;
    bool restored_ = false, combined_ = false, allowSiren_ = false;
    unsigned Duration(unsigned definition) const {
        return definition == Symbol2527::Buff                  ? unsigned(Symbol2527::DurationMs)
               : definition == PersonalSwiftness4284::Buff     ? unsigned(PersonalSwiftness4284::DurationMs)
               : definition == Protection965::Buff             ? unsigned(Protection965::DurationMs)
               : definition == RockBarrier6224::Buff           ? unsigned(RockBarrier6224::DurationMs)
               : definition == Phoenix5258::Buff               ? unsigned(Phoenix5258::DurationMs)
               : allowSiren_ && definition == Siren4606::Might ? Siren4606::Duration(definition)
                                                               : 0;
    }
    static bool Number(const std::string& text, uint64_t& value) {
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
    std::string Rows() const {
        auto text = "1:" + std::to_string(next_) + '|';
        for (const auto& boon : boons_) {
            if (text.back() != '|')
                text += ',';
            text += std::to_string(boon.id) + ':' + std::to_string(boon.definition) + ':' + std::to_string(boon.start) +
                    ':' + std::to_string(boon.end);
        }
        return text;
    }
    bool Create(uint16_t actor, const Boon& boon, uint64_t now, unsigned level, Buffer& out) const {
        if (now >= boon.endsAt || level > 80 || !actor)
            return false;
        return out.U16(create_) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(boon.definition) &&
               out.VarU32(boon.id) && out.VarU32(static_cast<uint32_t>(boon.endsAt - now)) &&
               out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    bool Remove(uint16_t actor, const Boon& boon, Buffer& out) const {
        return actor && out.U16(remove_) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(boon.id);
    }

  public:
    explicit PositiveEffects(uint16_t create = Rules::kCreate, uint16_t remove = Rules::kRemove,
                             bool allowSiren = false)
        : stance_(create, remove), create_(create), remove_(remove), allowSiren_(allowSiren) {}
    bool Load(const Character& saved, unsigned profession) {
        PositiveEffects candidate(create_, remove_, allowSiren_);
        Character legacy = saved;
        const auto entry = saved.profileFields.find("skill_effects");
        if (entry == saved.profileFields.end() || entry->second.compare(0, 3, "b1:")) {
            if (!candidate.stance_.Load(saved, profession))
                return false;
        } else {
            StoredPositiveEffects stored;
            if (!ReadPositiveEffects(entry->second, stored))
                return false;
            legacy.profileFields["skill_effects"] = stored.stance;
            if (!candidate.stance_.Load(legacy, profession))
                return false;
            candidate.regeneration_ = std::move(stored.regeneration);
            candidate.next_ = stored.next;
            candidate.combined_ = true;
            for (const auto& effect : stored.effects) {
                if (!Duration(effect.definition) || effect.end - effect.start > Duration(effect.definition))
                    return false;
                Boon boon;
                boon.id = effect.id;
                boon.definition = effect.definition;
                boon.start = effect.start;
                boon.end = effect.end;
                candidate.boons_.push_back(boon);
            }
        }
        unsigned rocks = 0;
        for (const auto& boon : candidate.boons_)
            if (boon.definition == RockBarrier6224::Buff)
                ++rocks;
        const auto owner = saved.profileFields.find("profession");
        if (rocks && (rocks > RockBarrier6224::Charges || owner == saved.profileFields.end() || owner->second != "6"))
            return false;
        for (const auto& boon : candidate.boons_)
            if (boon.definition == Phoenix5258::Buff &&
                !(allowSiren_ && boon.end - boon.start <= Siren4606::Duration(Siren4606::Vigor)) &&
                (owner == saved.profileFields.end() || owner->second != "6"))
                return false;
        *this = std::move(candidate);
        return true;
    }
    std::string Text() const {
        const auto stance = stance_.Text();
        if (!combined_)
            return stance;
        return "b1:" + std::to_string(stance.size()) + ':' + stance + regeneration_.Text(Rows());
    }
    bool Restore(uint64_t now, uint64_t utc) {
        if (restored_)
            return true;
        if (!stance_.Restore(now, utc))
            return false;
        for (auto& boon : boons_) {
            if (!utc)
                return false;
            const auto remaining = boon.end > utc ? boon.end - utc : 0;
            if (remaining > Duration(boon.definition) || now > UINT64_MAX - remaining)
                return false;
            boon.endsAt = now + remaining;
        }
        restored_ = true;
        return true;
    }
    bool Start(uint16_t actor, unsigned skill, uint64_t now, uint64_t utc, unsigned level, Buffer& bytes,
               uint64_t elapsed = 0) {
        return stance_.Start(actor, skill, now, utc, level, bytes, elapsed);
    }
    unsigned ActiveBuff(uint64_t now) const {
        return stance_.ActiveBuff(now);
    }
    bool HasRegeneration() const {
        return regeneration_.Enabled();
    }
    bool ActiveStance(uint64_t now) const {
        return stance_.ActiveStance(now);
    }
    bool Consume(uint16_t actor, Buffer& bytes) {
        return stance_.Consume(actor, bytes);
    }
    unsigned RockCount(uint64_t now) const {
        unsigned count = 0;
        for (const auto& b : boons_)
            if (b.definition == RockBarrier6224::Buff && now < b.endsAt)
                ++count;
        return count;
    }
    uint64_t RockDeadline() const {
        uint64_t end = 0;
        for (const auto& b : boons_)
            if (b.definition == RockBarrier6224::Buff)
                end = (std::max)(end, b.end);
        return end;
    }
    bool StartRocks(uint16_t actor, const WorldActionContext& c, unsigned level, Buffer& bytes) {
        if (RockDeadline() || !c.wallNow || c.wallNow > UINT64_MAX - RockBarrier6224::DurationMs)
            return false;
        ReceivedWorldEffect request;
        request.definition = RockBarrier6224::Buff;
        request.endsUtc = c.wallNow + RockBarrier6224::DurationMs;
        for (unsigned i = 0; i < RockBarrier6224::Charges; ++i) {
            unsigned id = 0;
            if (!Receive(actor, level, c, request, id, bytes))
                return false;
        }
        return true;
    }
    bool ConsumeRock(uint16_t actor, uint64_t now, Buffer& bytes) {
        for (auto at = boons_.end(); at != boons_.begin();) {
            --at;
            if (at->definition == RockBarrier6224::Buff && now < at->endsAt) {
                if (at->announced && !Remove(actor, *at, bytes))
                    return false;
                boons_.erase(at);
                return true;
            }
        }
        return false;
    }
    bool StartPersonal(uint16_t actor, unsigned skill, const WorldActionContext& c, unsigned level, Buffer& bytes) {
        const unsigned duration = skill == PersonalSwiftness4284::Skill ? unsigned(PersonalSwiftness4284::DurationMs)
                                  : skill == Phoenix5258::Skill         ? unsigned(Phoenix5258::DurationMs)
                                                                        : 0;
        if (!duration || !c.wallNow || c.wallNow > UINT64_MAX - duration)
            return false;
        ReceivedWorldEffect request;
        request.definition =
            skill == Phoenix5258::Skill ? unsigned(Phoenix5258::Buff) : unsigned(PersonalSwiftness4284::Buff);
        request.endsUtc = c.wallNow + duration;
        unsigned instance = 0;
        return Receive(actor, level, c, request, instance, bytes);
    }
    bool Snapshot(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes) const {
        if (!stance_.Snapshot(actor, now, level, bytes))
            return false;
        for (const auto& boon : boons_)
            if (boon.announced && now < boon.endsAt && !Create(actor, boon, now, level, bytes))
                return false;
        return true;
    }
    bool Advance(uint16_t actor, uint64_t now, unsigned level, Buffer& bytes, bool& changed) {
        if (!stance_.Advance(actor, now, level, bytes, changed))
            return false;
        for (auto at = boons_.begin(); at != boons_.end();) {
            if (now >= at->endsAt) {
                if (at->announced && !Remove(actor, *at, bytes))
                    return false;
                changed = changed || at->announced;
                at = boons_.erase(at);
            } else {
                if (!at->announced) {
                    if (!Create(actor, *at, now, level, bytes))
                        return false;
                    at->announced = true;
                    changed = true;
                }
                ++at;
            }
        }
        return true;
    }
    // Call on a disposable copy BEFORE Advance removes just-expired rows.
    bool PrepareHealth(unsigned build, const WorldActionContext& c, std::string& health, bool& changed) {
        changed = false;
        health = c.healthState;
        if (!regeneration_.Enabled())
            return true;
        float current = 0, delta = 0;
        if (!modern::ReadHealthState(health, current, BetaHealthMaximum(build)) ||
            !regeneration_.Prepare(Rows(), c.wallNow, current, BetaHealthMaximum(build), delta, changed))
            return false;
        return !delta || modern::WriteHealthState(current, health, BetaHealthMaximum(build));
    }
    bool Receive(uint16_t actor, unsigned level, const WorldActionContext& c, const ReceivedWorldEffect& request,
                 unsigned& instance, Buffer& bytes) {
        const auto duration = Duration(request.definition);
        if (!c.wallNow || !duration ||
            (request.definition == 289 && (!request.sourceLevel || request.sourceLevel > 80)) ||
            request.endsUtc <= c.wallNow || request.endsUtc - c.wallNow > duration || request.endsUtc <= duration ||
            c.now > UINT64_MAX - (request.endsUtc - c.wallNow) || !Restore(c.now, c.wallNow))
            return false;
        auto found = boons_.end();
        if (request.refreshInstance) {
            found = std::find_if(boons_.begin(), boons_.end(),
                                 [&](const Boon& b) { return b.id == request.refreshInstance; });
            if (found == boons_.end() || found->definition != request.definition || found->end <= c.wallNow)
                return false;
        } else if (boons_.size() >= 64 || next_ == UINT32_MAX)
            return false;
        Boon boon;
        boon.id = request.refreshInstance ? request.refreshInstance : next_;
        boon.definition = request.definition;
        boon.start = allowSiren_ && request.definition == Siren4606::Vigor &&
                             request.endsUtc - c.wallNow <= Siren4606::Duration(Siren4606::Vigor)
                         ? c.wallNow
                         : request.endsUtc - duration;
        boon.end = request.endsUtc;
        boon.endsAt = c.now + (request.endsUtc - c.wallNow);
        boon.announced = true;
        if ((request.definition == 289 && !regeneration_.Grant(boon.id, request.sourceLevel, boon.start, boon.end)) ||
            (found != boons_.end() && found->announced && !Remove(actor, *found, bytes)) ||
            !Create(actor, boon, c.now, level, bytes))
            return false;
        if (found == boons_.end()) {
            boons_.push_back(boon);
            ++next_;
        } else
            *found = boon;
        combined_ = true;
        instance = boon.id;
        return true;
    }
};

template <class Effects>
WorldActionResult PreparePositiveEffects(Effects& live, uint64_t& version, uint16_t actor, unsigned level,
                                         const WorldActionContext& c, const std::vector<ReceivedWorldEffect>& requests,
                                         std::unique_ptr<PreparedWorldEffect>& out, bool allowSiren = false) {
    if (requests.empty() || requests.size() > 5 || c.peerAgent != actor || version == UINT64_MAX)
        return WorldActionResult::Failed;
    auto candidate = live;
    WorldActionOutput output;
    output.sequences = 0;
    std::vector<unsigned> instances;
    std::set<unsigned> refreshed;
    for (const auto& request : requests) {
        const auto sirenDuration = allowSiren ? Siren4606::Duration(request.definition) : 0;
        const bool siren = sirenDuration && request.endsUtc > c.wallNow && request.endsUtc - c.wallNow <= sirenDuration;
        if ((request.definition != 289 && request.definition != Protection965::Buff && !siren) ||
            (request.refreshInstance && !refreshed.insert(request.refreshInstance).second))
            return WorldActionResult::Failed;
        unsigned instance = 0;
        Buffer bytes;
        if (!candidate.Receive(actor, level, c, request, instance, bytes) || !output.owned.Append(bytes.View()) ||
            !output.peers.Append(bytes.View()))
            return WorldActionResult::Failed;
        instances.push_back(instance);
    }
    struct Prepared final : PreparedWorldEffect {
        Effects& live;
        uint64_t& version;
        Effects candidate;
        Prepared(Effects& l, uint64_t& v, Effects&& n) : live(l), version(v), candidate(std::move(n)) {}
        void Commit() noexcept override {
            live = std::move(candidate);
            ++version;
        }
    };
    std::unique_ptr<Prepared> prepared(new Prepared(live, version, std::move(candidate)));
    prepared->before = c.skillEffects;
    prepared->after = prepared->candidate.Text();
    if (prepared->after.size() > 8192)
        return WorldActionResult::Failed;
    prepared->instance = instances.front();
    prepared->instances = std::move(instances);
    output.peerEffectVersion = version + 1;
    prepared->output = std::move(output);
    out = std::move(prepared);
    return WorldActionResult::Handled;
}
} // namespace beta
} // namespace glide
