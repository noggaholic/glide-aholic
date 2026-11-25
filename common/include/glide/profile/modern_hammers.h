#include "glide/profile/boon_removal.h"
#pragma once
#include "glide/profile/modern_energy.h"
#include "glide/profile/modern_condition_data.h"
#include "glide/profile/modern_health.h"
#include "glide/profile/dragon_facets.h"
#include "glide/profile/regeneration.h"
#include <string>

namespace glide {
namespace modern {
// H1 remains the original Hammers envelope. H2:28287:applied: followed by its six ledger fields identifies the
// personal demon elite independently. applied records the first activation, not whether this connection announced
// the buff. HoT consumes it in the compound condition/energy/effect Tick save. H2:27107:0:<six ledger
// fields>:aux1:aux2:pulseUtc preserves this action's private HoT contributions. Modern aux/pulse are zero. H1
// and28287 stay exact. Immediate on/off and HoT1s cadence are local policy; skill 27581 retains10s and1187/5974
// retain1s deadlines. These are personal cycles only; no hits, periodic damage or stat modifiers.
template <unsigned Build> class Hammers {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported Hammers dialect");
    uint16_t actor_ = 57344;
    bool facetMode_ = false;
    DragonFacets<Build> facets_;
    combat::BoonRemovals removedBoons_;
    bool tracked_ = false, restored_ = false, announced_ = false, needsSave_ = false;
    uint32_t instance_ = 0;
    uint64_t startsUtc_ = 0, startsAt_ = 0;
    unsigned skill_ = 26557;
    bool activationApplied_ = false;
    uint32_t auxiliary1_ = 0, auxiliary2_ = 0;
    uint64_t pulseUtc_ = 0, shortEndsAt_ = 0, markerEndsAt_ = 0;
    unsigned savedAmount_ = 0;
    int savedRate_ = 5;
    uint64_t savedUtc_ = 0, savedUpkeepUtc_ = 0;
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
    static bool CreateDefinition(uint16_t actor, uint32_t instance, unsigned definition, unsigned duration,
                                 unsigned level, Buffer& out) {
        return actor && instance && level <= UINT8_MAX &&
               out.U16(Build == 49284    ? 497
                       : Build == 167409 ? 713
                                         : 735) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(definition) && out.VarU32(instance) &&
               out.VarU32(duration) && out.U8(static_cast<uint8_t>(level)) && out.U8(1);
    }
    bool Create(uint16_t actor, uint32_t instance, unsigned level, Buffer& out) const {
        return CreateDefinition(actor, instance, CurrentBuff(), 0, level, out);
    }
    static bool Remove(uint16_t actor, uint32_t instance, Buffer& out) {
        return actor && instance &&
               out.U16(Build == 49284    ? 500
                       : Build == 167409 ? 717
                                         : 739) &&
               out.VarU32(actor) && out.VarU32(actor) && out.VarU32(instance);
    }
    bool RemoveOwn(uint16_t actor, Buffer& out) const {
        return Remove(actor, instance_, out) &&
               (!auxiliary1_ || removedBoons_.Contains(auxiliary1_, 1187) || Remove(actor, auxiliary1_, out)) &&
               (!auxiliary2_ || Remove(actor, auxiliary2_, out));
    }
    void Clear() {
        instance_ = auxiliary1_ = auxiliary2_ = 0;
        startsUtc_ = startsAt_ = pulseUtc_ = shortEndsAt_ = markerEndsAt_ = 0;
        announced_ = activationApplied_ = false;
    }
    bool PulseSnapshot(uint16_t actor, uint64_t now, unsigned level, Buffer& out) const {
        return (markerEndsAt_ <= now ||
                CreateDefinition(actor, instance_, 27581, static_cast<unsigned>(markerEndsAt_ - now), level, out)) &&
               (shortEndsAt_ <= now ||
                ((removedBoons_.Contains(auxiliary1_, 1187) ||
                  CreateDefinition(actor, auxiliary1_, 1187, static_cast<unsigned>(shortEndsAt_ - now), level, out)) &&
                 CreateDefinition(actor, auxiliary2_, 5974, static_cast<unsigned>(shortEndsAt_ - now), level, out)));
    }

  public:
    explicit Hammers(uint16_t actor = 57344) : actor_(actor) {}
    enum : unsigned {
        Skill = 26557,
        StopSkill = 26956,
        Buff = 27273,
        Slot = 3,
        Legend = Build == 49284    ? 13
                 : Build == 167409 ? 16
                                   : 17,
        ActivationMs = 1350,
        AnimationMs = 2067
    };
    // Off activation0 is local policy; the original animation trigger1200 does not prove removal.
    static bool IsEliteSkill(unsigned skill) {
        return skill == 28287 || skill == 26693;
    }
    static bool IsAssassinSkill(unsigned skill) {
        return skill == 27107 || skill == 28382;
    }
    static bool IsMaintainedSkill(unsigned skill) {
        return IsEliteSkill(skill) || IsAssassinSkill(skill) || skill == Skill || skill == StopSkill;
    }
    static unsigned StartFor(unsigned skill) {
        if (FacetData<Build>::IsFacetSkill(skill))
            return FacetData<Build>::BaseFor(skill);
        return IsAssassinSkill(skill) ? 27107 : IsEliteSkill(skill) ? 28287 : Skill;
    }
    static unsigned StopFor(unsigned skill) {
        if (FacetData<Build>::IsFacetSkill(skill))
            return FacetData<Build>::RuleFor(skill).alternate;
        return IsAssassinSkill(skill) ? 28382 : IsEliteSkill(skill) ? 26693 : StopSkill;
    }
    static unsigned LegendFor(unsigned skill) {
        if (FacetData<Build>::IsFacetSkill(skill))
            return FacetData<Build>::Legend;
        return IsAssassinSkill(skill) ? Legend - 1 : Legend + (IsEliteSkill(skill) ? 1u : 0u);
    }
    static unsigned BuffFor(unsigned skill) {
        if (FacetData<Build>::IsFacetSkill(skill))
            return FacetData<Build>::RuleFor(skill).marker;
        return IsAssassinSkill(skill) ? 27581 : IsEliteSkill(skill) ? 28001 : Buff;
    }
    static unsigned ActivationFor(unsigned skill, bool water) {
        return IsAssassinSkill(skill) || skill == 26693 || skill == StopSkill ? 0u
               : IsEliteSkill(skill)                                          ? (Build == 49284 || water ? 1200u : 640u)
                                                                              : ActivationMs;
    }
    static unsigned AnimationFor(unsigned skill, bool water) {
        return IsAssassinSkill(skill) ? 0u
               : skill == 26693       ? (water ? 2033u : 2500u)
               : IsEliteSkill(skill)  ? (Build == 49284 ? 2500u
                                         : water        ? 4333u
                                                        : 1250u)
                                      : AnimationMs;
    }
    static unsigned RechargeFor(unsigned skill) {
        return skill == 28287 && Build != 49284 ? 3000u : 0u;
    }
    static int RateFor(unsigned skill) {
        return Build == 49284 ? (IsAssassinSkill(skill) ? -5 : IsEliteSkill(skill) ? -3 : -2) : -1;
    }
    unsigned CurrentSkill() const {
        return facetMode_ ? 27220u : skill_;
    }
    unsigned CurrentBuff() const {
        return BuffFor(CurrentSkill());
    }
    bool IsElite() const {
        return !facetMode_ && IsEliteSkill(skill_);
    }
    bool IsAssassin() const {
        return !facetMode_ && IsAssassinSkill(skill_);
    }
    bool HasPulses() const {
        return Build == 49284 && IsAssassin();
    }
    uint64_t ActivationUtc() const {
        return startsUtc_;
    }
    bool InitialConditionDue(uint64_t now) const {
        return Build == 49284 && IsElite() && Active(now) && !activationApplied_;
    }
    void AppliedInitialCondition() {
        activationApplied_ = true;
    }
    bool SetRemovedBoons(const std::string& text) {
        return removedBoons_.Load(text);
    }
    void Boons(const WorldActionContext& c, std::vector<combat::LiveBoon>& out) const {
        if (HasPulses() && auxiliary1_ && pulseUtc_ && pulseUtc_ <= UINT64_MAX - 3000 && pulseUtc_ + 3000 > c.wallNow) {
            combat::LiveBoon r;
            r.instance = auxiliary1_;
            r.definition = 1187;
            r.starts = pulseUtc_;
            r.ends = pulseUtc_ + 3000;
            r.announced = announced_;
            out.push_back(r);
        }
    }
    bool Load(const Character& saved, Character& legacy) {
        *this = Hammers(actor_);
        legacy = saved;
        if (!removedBoons_.Load(combat::BoonRemovals::Field(saved)))
            return false;
        const auto found = saved.profileFields.find("skill_effects");
        if (found == saved.profileFields.end())
            return true;
        if (found->second.compare(0, 3, "H3:") == 0) {
            const auto profession = saved.profileFields.find("profession");
            const auto& text = found->second;
            const auto bar = text.find('|');
            if (profession == saved.profileFields.end() || profession->second != "9" || text.size() > 8192 ||
                bar == std::string::npos)
                return false;
            size_t cursor = 3;
            uint64_t amount = 0, utc = 0;
            int rate = 0;
            for (unsigned i = 0; i < 3; ++i) {
                const auto end = text.find(':', cursor);
                if (end == std::string::npos || end > bar)
                    return false;
                const auto field = text.substr(cursor, end - cursor);
                if (i == 1) {
                    uint64_t n = 0;
                    const bool negative = !field.empty() && field[0] == '-';
                    if (!Decimal(negative ? field.substr(1) : field, n) || n > 20 || (negative && !n))
                        return false;
                    rate = negative ? -int(n) : int(n);
                } else if (!Decimal(field, i ? utc : amount))
                    return false;
                cursor = end + 1;
            }
            if (amount > 50000 || !utc || rate > 5 || !facets_.Load(text.substr(cursor, bar - cursor), utc) ||
                rate != facets_.Rate())
                return false;
            const auto tail = text.substr(bar + 1);
            std::string legacyTail;
            if (!Regeneration::Legacy(tail, legacyTail) || !facets_.AllocatorValid(legacyTail))
                return false;
            savedAmount_ = unsigned(amount);
            savedRate_ = rate;
            savedUtc_ = utc;
            facetMode_ = tracked_ = true;
            legacy.profileFields["skill_effects"] = tail;
            return true;
        }
        const bool extended = found->second.compare(0, 3, "H2:") == 0;
        if (!extended && found->second.compare(0, 3, "H1:") != 0)
            return true;
        const auto profession = saved.profileFields.find("profession");
        if (profession == saved.profileFields.end() || profession->second != "9")
            return false;
        const auto& text = found->second;
        const auto bar = text.find('|');
        if (bar == std::string::npos || bar > 240)
            return false;
        uint64_t values[6]{};
        size_t cursor = 3;
        if (extended) {
            const auto actionEnd = text.find(':', cursor);
            if (actionEnd == std::string::npos)
                return false;
            const auto action = text.substr(cursor, actionEnd - cursor);
            if (action != "28287" && action != "27107")
                return false;
            cursor = actionEnd + 1;
            const auto flagEnd = text.find(':', cursor);
            if (flagEnd == std::string::npos)
                return false;
            const auto flag = text.substr(cursor, flagEnd - cursor);
            if (flag != "0" && flag != "1")
                return false;
            skill_ = action == "27107" ? 27107 : 28287;
            activationApplied_ = flag == "1";
            cursor = flagEnd + 1;
            if (IsAssassin() && activationApplied_)
                return false;
        }
        const int rate = RateFor(skill_);
        for (unsigned i = 0; i < 6; ++i) {
            const auto end = i == 5 && !IsAssassin() ? bar : text.find(':', cursor);
            if (end == std::string::npos || end > bar)
                return false;
            const auto part = text.substr(cursor, end - cursor);
            if (i == 1) {
                if (part == std::to_string(rate))
                    savedRate_ = rate;
                else if (part == "5")
                    savedRate_ = 5;
                else
                    return false;
            } else if (!Decimal(part, values[i]))
                return false;
            cursor = end + 1;
        }
        if (IsAssassin()) {
            uint64_t extra[3]{};
            for (unsigned i = 0; i < 3; ++i) {
                const auto end = i == 2 ? bar : text.find(':', cursor);
                if (end == std::string::npos || end > bar || !Decimal(text.substr(cursor, end - cursor), extra[i]))
                    return false;
                cursor = end + 1;
            }
            if (extra[0] >= kFirstConditionInstance || extra[1] >= kFirstConditionInstance)
                return false;
            auxiliary1_ = static_cast<uint32_t>(extra[0]);
            auxiliary2_ = static_cast<uint32_t>(extra[1]);
            pulseUtc_ = extra[2];
        }
        if (values[0] > 50000 || !values[2] || values[4] >= kFirstConditionInstance)
            return false;
        savedAmount_ = static_cast<unsigned>(values[0]);
        savedUtc_ = values[2];
        savedUpkeepUtc_ = values[3];
        instance_ = static_cast<uint32_t>(values[4]);
        startsUtc_ = values[5];
        if (!HasPulses() || !instance_) {
            if (auxiliary1_ || auxiliary2_ || pulseUtc_)
                return false;
        } else if (!auxiliary1_ || !auxiliary2_ || auxiliary1_ == auxiliary2_ || instance_ == auxiliary1_ ||
                   instance_ == auxiliary2_ || !pulseUtc_ || pulseUtc_ < startsUtc_ || pulseUtc_ > savedUtc_ ||
                   (pulseUtc_ - startsUtc_) % 1000 || pulseUtc_ > UINT64_MAX - 10000)
            return false;
        if (!instance_) {
            if (startsUtc_ || savedUpkeepUtc_ || savedRate_ != 5 || activationApplied_)
                return false;
        } else if (!startsUtc_ || (savedUpkeepUtc_ ? (startsUtc_ != savedUpkeepUtc_ || savedUpkeepUtc_ <= savedUtc_ ||
                                                      savedUpkeepUtc_ - savedUtc_ > (IsAssassin() ? 0u
                                                                                     : IsElite()  ? 1200u
                                                                                                  : ActivationMs) ||
                                                      savedRate_ != 5 || activationApplied_)
                                                   : (startsUtc_ > savedUtc_ || savedRate_ != rate)))
            return false;
        const auto serializedEffects = text.substr(bar + 1);
        std::string effects;
        if (!Regeneration::Legacy(serializedEffects, effects))
            return false;
        const auto separator = effects.find('|');
        uint64_t next = 0;
        if (effects.compare(0, 2, "1:") || separator == std::string::npos ||
            !Decimal(effects.substr(2, separator - 2), next) || !next || next > UINT32_MAX ||
            (instance_ && instance_ >= next) || auxiliary1_ >= next || auxiliary2_ >= next)
            return false;
        // The maintained instance shares the existing allocator, never a timed instance.
        size_t at = separator + 1;
        while (at < effects.size()) {
            const auto colon = effects.find(':', at);
            uint64_t id = 0;
            if (colon == std::string::npos || !Decimal(effects.substr(at, colon - at), id) ||
                (instance_ && id == instance_) || (auxiliary1_ && id == auxiliary1_) ||
                (auxiliary2_ && id == auxiliary2_))
                return false;
            const auto comma = effects.find(',', colon);
            if (comma == std::string::npos)
                break;
            at = comma + 1;
        }
        legacy.profileFields["skill_effects"] = serializedEffects;
        tracked_ = true;
        return true;
    }
    bool Restore(const WorldActionContext& c, Energy<Build>& energy) {
        if (restored_)
            return true;
        if (facetMode_) {
            bool changed = false;
            if (!facets_.Restore(savedAmount_, savedRate_, savedUtc_, c, energy, changed))
                return false;
            needsSave_ = changed;
            restored_ = true;
            return true;
        }
        if (tracked_) {
            if (!energy.RestoreLedger(savedAmount_, savedRate_, savedUtc_, savedUpkeepUtc_, c.now, c.wallNow,
                                      RateFor(skill_),
                                      IsAssassin() ? 0u
                                      : IsElite()  ? 1200u
                                                   : ActivationMs))
                return false;
            const auto delay = startsUtc_ > c.wallNow ? startsUtc_ - c.wallNow : 0;
            if (delay > (IsAssassin() ? 0u : IsElite() ? 1200u : ActivationMs) || c.now > UINT64_MAX - delay)
                return false;
            startsAt_ = instance_ ? c.now + delay : 0;
            if (instance_ && energy.ExpiredOnRestore()) {
                Clear();
                needsSave_ = true;
            }
        }
        restored_ = true;
        return true;
    }
    bool ClockValid(const WorldActionContext& c) const {
        return facetMode_ ? c.wallNow != 0
                          : !tracked_ || (c.wallNow && (!instance_ || startsAt_ <= c.now ||
                                                        c.wallNow <= UINT64_MAX - (startsAt_ - c.now)));
    }
    bool Tracked() const {
        return tracked_;
    }
    bool NeedsSave() const {
        return needsSave_;
    }
    void Saved() {
        needsSave_ = false;
    }
    bool Present() const {
        return facetMode_ ? facets_.Present() : instance_ != 0;
    }
    bool Active(uint64_t now) const {
        if (facetMode_) {
            for (const auto& e : facets_.entries)
                if (e.marker && e.active && now >= e.at)
                    return true;
            return false;
        }
        return instance_ && now >= startsAt_;
    }
    static bool IsFacetSkill(unsigned skill) {
        return FacetData<Build>::IsFacetSkill(skill);
    }
    static unsigned FacetBaseFor(unsigned skill) {
        return FacetData<Build>::BaseFor(skill);
    }
    bool IsFacetMode() const {
        return facetMode_;
    }
    bool HasFacet(unsigned base) const {
        const auto* e = facets_.Find(base);
        return facetMode_ && e && e->marker;
    }
    bool FacetActive(unsigned base) const {
        const auto* e = facets_.Find(base);
        return facetMode_ && e && e->marker && e->active;
    }
    bool ScheduleFacet(unsigned base, unsigned marker, const WorldActionContext& c, Energy<Build>& energy,
                       bool water = false) {
        if (!FacetData<Build>::IsBase(base) || instance_)
            return false;
        auto facets = facets_;
        if (!facets.Schedule(base, marker, c, water))
            return false;
        if (!facetMode_ && !energy.SetFacetRate(c.now, 5))
            return false;
        facets_ = std::move(facets);
        facetMode_ = tracked_ = restored_ = true;
        needsSave_ = true;
        return true;
    }
    bool ConsumeFacet(unsigned base, const WorldActionContext& c, Energy<Build>& energy, WorldActionOutput& out) {
        const auto* e = facets_.Find(base);
        if (!facetMode_ || !e || !e->marker || !e->active)
            return false;
        const auto old = *e;
        if (!facets_.Consume(base, c.now, energy))
            return false;
        if (old.announced && (!Remove(actor_, old.marker, out.owned) || !Remove(c.peerAgent, old.marker, out.peers)))
            return false;
        needsSave_ = true;
        return energy.Advance(c.now, out);
    }
    FacetPulseStatus PrepareFacetPulse(const WorldActionContext& c, const std::vector<WorldConditionActor>& allies,
                                       FacetPulsePlan& plan) const {
        return facetMode_ ? facets_.Prepare(c, allies, plan) : FacetPulseStatus::None;
    }
    bool CommitFacetPulse(const FacetPulsePlan& plan, const std::vector<unsigned>& own,
                          const std::vector<unsigned>& allies) {
        if (!facetMode_ || !facets_.Commit(plan, own, allies))
            return false;
        needsSave_ = true;
        return true;
    }
    bool AdvanceFacets(const WorldActionContext& c, Energy<Build>& energy, WorldActionOutput& out, bool& changed,
                       bool cancel, bool publish, unsigned level) {
        const auto before = facets_.entries;
        if (!facets_.Advance(c, energy, cancel, changed))
            return false;
        for (const auto& old : before)
            if (old.marker && old.announced) {
                const auto* e = facets_.Find(old.base);
                if ((!e || old.marker != e->marker) &&
                    (!Remove(actor_, old.marker, out.owned) || !Remove(c.peerAgent, old.marker, out.peers)))
                    return false;
            }
        if (publish)
            for (auto& e : facets_.entries)
                if (e.marker && e.active && !e.announced) {
                    if (!CreateDefinition(actor_, e.marker, FacetData<Build>::RuleFor(e.base).marker, 0, level,
                                          out.owned) ||
                        !CreateDefinition(c.peerAgent, e.marker, FacetData<Build>::RuleFor(e.base).marker, 0, level,
                                          out.peers))
                        return false;
                    e.announced = true;
                    changed = true;
                }
        if (changed) {
            needsSave_ = true;
            if (!energy.Advance(c.now, out))
                return false;
        }
        return true;
    }
    bool Schedule(uint32_t instance, const WorldActionContext& c, Energy<Build>& energy, unsigned skill = Skill,
                  bool water = false, uint32_t auxiliary1 = 0, uint32_t auxiliary2 = 0) {
        if (!IsMaintainedSkill(skill) || skill != StartFor(skill) || (facetMode_ && facets_.Present()))
            return false;
        if (facetMode_) {
            facetMode_ = false;
            facets_ = DragonFacets<Build>{};
        }
        const auto delay = ActivationFor(skill, water);
        const bool pulses = Build == 49284 && IsAssassinSkill(skill);
        if (pulses ? (!auxiliary1 || !auxiliary2 || auxiliary1 == auxiliary2 || auxiliary1 == instance ||
                      auxiliary2 == instance || auxiliary1 >= kFirstConditionInstance ||
                      auxiliary2 >= kFirstConditionInstance || c.wallNow > UINT64_MAX - 10000 ||
                      c.now > UINT64_MAX - 10000)
                   : (auxiliary1 || auxiliary2))
            return false;
        if (instance_ || !instance || instance >= kFirstConditionInstance || !c.wallNow ||
            c.wallNow > UINT64_MAX - delay || !energy.BeginUpkeep(c.now, delay, RateFor(skill)))
            return false;
        tracked_ = true;
        restored_ = true;
        announced_ = false;
        activationApplied_ = false;
        skill_ = skill;
        instance_ = instance;
        auxiliary1_ = auxiliary1;
        auxiliary2_ = auxiliary2;
        pulseUtc_ = shortEndsAt_ = markerEndsAt_ = 0;
        startsUtc_ = c.wallNow + delay;
        startsAt_ = c.now + delay;
        return true;
    }
    std::string Text(const std::string& legacy, const Energy<Build>& energy, const WorldActionContext& c) const {
        if (!tracked_)
            return legacy;
        if (facetMode_)
            return "H3:" + std::to_string(energy.Available(c.now)) + ':' + std::to_string(energy.CurrentRate(c.now)) +
                   ':' + std::to_string(c.wallNow) + ':' + facets_.Text() + '|' + legacy;
        const auto delay = energy.UpkeepDelay(c.now), pending = delay ? c.wallNow + delay : 0;
        const auto start = instance_ ? (pending ? pending : (std::min)(startsUtc_, c.wallNow)) : 0;
        const auto prefix = IsElite()      ? "H2:28287:" + std::to_string(activationApplied_ ? 1 : 0) + ':'
                            : IsAssassin() ? std::string("H2:27107:0:")
                                           : std::string("H1:");
        const auto extra = IsAssassin() ? ':' + std::to_string(auxiliary1_) + ':' + std::to_string(auxiliary2_) + ':' +
                                              std::to_string(pulseUtc_)
                                        : std::string();
        return prefix + std::to_string(energy.Available(c.now)) + ':' + std::to_string(energy.CurrentRate(c.now)) +
               ':' + std::to_string(c.wallNow) + ':' + std::to_string(pending) + ':' + std::to_string(instance_) + ':' +
               std::to_string(start) + extra + '|' + legacy;
    }
    bool Stop(const WorldActionContext& c, Energy<Build>& energy, WorldActionOutput& out) {
        if (facetMode_) {
            bool changed = false;
            return AdvanceFacets(c, energy, out, changed, true, false, 0);
        }
        if (!instance_)
            return true;
        if (announced_ && (!RemoveOwn(actor_, out.owned) || !RemoveOwn(c.peerAgent, out.peers)))
            return false;
        if (!energy.EndUpkeep(c.now, out))
            return false;
        Clear();
        return true;
    }
    bool Advance(const WorldActionContext& c, unsigned legend, unsigned level, Energy<Build>& energy,
                 WorldActionOutput& out, bool& changed, bool publishInitial = true) {
        if (facetMode_) {
            float health = 0;
            if (!ReadHealthState(c.healthState, health))
                return false;
            return AdvanceFacets(c, energy, out, changed, health == 0 || legend != FacetData<Build>::Legend,
                                 publishInitial, level);
        }
        changed = false;
        if (!instance_)
            return true;
        float health = 0;
        if (!ReadHealthState(c.healthState, health))
            return false;
        uint64_t exhaustion = 0;
        if (legend != LegendFor(skill_) || health == 0 || energy.Exhaustion(c.now, exhaustion)) {
            if (!Stop(c, energy, out))
                return false;
            changed = true;
            return true;
        }
        if (HasPulses() && c.now >= startsAt_) {
            // Ordinary requests can settle depletion without refreshing a
            // contribution just before an off request. Tick/new on publishes.
            if (!publishInitial)
                return true;
            if (!c.wallNow || c.wallNow < startsUtc_ || c.wallNow < pulseUtc_)
                return false;
            const auto nominal = c.wallNow - (c.wallNow - startsUtc_) % 1000;
            if (!announced_ || nominal > pulseUtc_) {
                const auto elapsed = c.wallNow - nominal;
                if (nominal > UINT64_MAX - 10000 || c.now > UINT64_MAX - (10000 - elapsed))
                    return false;
                if (announced_ && (!RemoveOwn(actor_, out.owned) || !RemoveOwn(c.peerAgent, out.peers)))
                    return false;
                pulseUtc_ = nominal;
                shortEndsAt_ = c.now + (1000 - elapsed);
                markerEndsAt_ = c.now + (10000 - elapsed);
                if (!PulseSnapshot(actor_, c.now, level, out.owned) ||
                    !PulseSnapshot(c.peerAgent, c.now, level, out.peers))
                    return false;
                announced_ = true;
                changed = true;
            }
            return true;
        }
        if (!announced_ && c.now >= startsAt_) {
            if (!publishInitial && InitialConditionDue(c.now))
                return true;
            if (!Create(actor_, instance_, level, out.owned) || !Create(c.peerAgent, instance_, level, out.peers))
                return false;
            announced_ = true;
            changed = true;
            if (!InitialConditionDue(c.now))
                activationApplied_ = IsElite();
        }
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, unsigned level, const Energy<Build>& energy, Buffer& out) const {
        if (facetMode_) {
            uint64_t exhausted = 0;
            if (energy.Exhaustion(now, exhausted))
                return true;
            for (const auto& e : facets_.entries)
                if (e.marker && e.active && e.announced &&
                    !CreateDefinition(actor, e.marker, FacetData<Build>::RuleFor(e.base).marker, 0, level, out))
                    return false;
            return true;
        }
        return !announced_ || !instance_ || now < startsAt_ || !energy.Available(now) ||
               (HasPulses() ? PulseSnapshot(actor, now, level, out) : Create(actor, instance_, level, out));
    }
    bool Cleanup(uint16_t actor, Buffer& out) const {
        if (facetMode_) {
            for (const auto& e : facets_.entries)
                if (e.marker && e.announced && !Remove(actor, e.marker, out))
                    return false;
            return true;
        }
        return !instance_ || !announced_ || RemoveOwn(actor, out);
    }
};
} // namespace modern
} // namespace glide
