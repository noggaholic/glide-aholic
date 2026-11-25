#pragma once
#include "glide/profile/modern_energy_costs.h"
#include "glide/profile/hot_energy_costs.h"
#include "glide/profile/modern_demon_healing_data.h"
#include "glide/profile/modern_great_dwarf_data.h"
#include "glide/profile/world_actions.h"
#include "glide/profile/dragon_facet_data.h"
#include <algorithm>

namespace glide {
namespace modern {
// Exploration resource policy: start and cap at 50, recover 5 points/second. Modern resting HUDs double the wire
// maximum; HoT uses it directly. Legend changes reset this resource. The audited modern Hammers action adds an
// optional signed upkeep schedule. HoT publishes its local cap50 directly; this deliberately makes its native
// ratio meter full at50 (possibly100%). Keep integer thousandths to avoid accumulated tick rounding errors.
template <unsigned Build> class Energy {
    static_assert(Build == 49284 || Build == 167409 || Build == 204489, "Unsupported energy dialect");
    bool enabled_ = false, started_ = false, reported_ = false;
    uint16_t actor_ = 57344;
    uint64_t at_ = 0, reportedAt_ = 0;
    unsigned amount_ = 50000;
    int rate_ = 5, reportedRate_ = 5;
    uint64_t upkeepAt_ = 0;
    bool managed_ = false, expiredOnRestore_ = false;
    static constexpr int UpkeepRate = Build == 49284 ? -2 : -1;
    int upkeepRate_ = UpkeepRate;
    static bool ValidUpkeep(int rate) {
        return rate == UpkeepRate || (Build == 49284 && (rate == -3 || rate == -5));
    }
    uint64_t DrainTime(unsigned amount) const {
        return (uint64_t(amount) + unsigned(-upkeepRate_) - 1) / unsigned(-upkeepRate_);
    }
    static bool Cost(uint32_t skill, unsigned& cost) {
        // Both own modern records give Empowering Misery a cost of five.
        return skill == 26679                                         ? (cost = Build == 49284 ? 50u : 10u, true)
               : skill == 28406                                       ? (cost = 50, true)
               : skill == 27505                                       ? (cost = 20, true)
               : (skill == 26937)                                     ? (cost = 5, true)
               : FacetData<Build>::IsFacetSkill(skill)                ? (cost = 0, true)
               : skill == 27917                                       ? (cost = Build == 49284 ? 35u : 30u, true)
               : skill == 29209                                       ? (cost = Build == 49284 ? 35u : 30u, true)
               : skill == 28231                                       ? (cost = Build == 49284 ? 20u : 30u, true)
               : skill == 28516                                       ? (cost = Build == 49284 ? 25u : 30u, true)
               : skill == 27322                                       ? (cost = Build == 49284 ? 35u : 30u, true)
               : (skill == 26557 || skill == 28287 || skill == 27107) ? (cost = 5, true)
               : (skill == 26956 || skill == 26693 || skill == 28382) ? (cost = 0, true)
               : Build == 49284                                       ? hot49284::InitialEnergyCost(skill, cost)
               : skill == DemonHealingData<204489>::Skill             ? (cost = 5, true)
               : skill == GreatDwarfData<204489>::Skill ? (cost = GreatDwarfData<204489>::EnergyCost, true)
                                                        : InitialEnergyCost(Build, skill, cost);
    }
    static unsigned Accrue(unsigned amount, int rate, uint64_t elapsed) {
        // Each elapsed millisecond changes thousandths by the points/second rate.
        // Saturate before multiplication, including long gaps and negative upkeep.
        const int64_t delta = static_cast<int64_t>((std::min)(elapsed, uint64_t(100000))) * rate;
        return static_cast<unsigned>((std::max)(int64_t(0), (std::min)(int64_t(50000), int64_t(amount) + delta)));
    }
    int Rate(uint64_t now) const {
        return upkeepAt_ && now >= upkeepAt_ ? upkeepRate_ : rate_;
    }
    unsigned Value(uint64_t now) const {
        if (!started_ || now <= at_)
            return amount_;
        if (upkeepAt_ && now >= upkeepAt_) {
            const auto until = upkeepAt_ > at_ ? upkeepAt_ - at_ : 0;
            return Accrue(Accrue(amount_, rate_, until), upkeepRate_, now - (std::max)(at_, upkeepAt_));
        }
        return Accrue(amount_, rate_, now - at_);
    }
    void Capture(uint64_t now) {
        amount_ = Value(now);
        rate_ = Rate(now);
        if (upkeepAt_ && now >= upkeepAt_)
            upkeepAt_ = 0;
        at_ = started_ ? (std::max)(at_, now) : now;
        started_ = true;
    }
    bool Emit(uint64_t now, WorldActionOutput& out) {
        Buffer packet;
        if (!packet.U16(Build == 49284    ? 340
                        : Build == 167409 ? 476
                                          : 496) ||
            !packet.VarU32(actor_) || !packet.F32(static_cast<float>(amount_) / 1000) || !packet.F32(50) ||
            !packet.F32(static_cast<float>(Rate(now))) || !out.owned.Append(packet.View()))
            return false;
        reported_ = true;
        reportedAt_ = now;
        reportedRate_ = Rate(now);
        return true;
    }

  public:
    explicit Energy(bool enabled = false, uint16_t actor = 57344) : enabled_(enabled), actor_(actor) {}
    bool Reset(uint64_t now, WorldActionOutput& out) {
        if (!enabled_)
            return true;
        amount_ = 50000;
        at_ = now;
        started_ = true;
        rate_ = 5;
        upkeepAt_ = 0;
        return Emit(now, out);
    }
    bool Enabled() const {
        return enabled_;
    }
    bool Managed() const {
        return managed_;
    }
    unsigned Available(uint64_t now) const {
        return Value(now);
    }
    int CurrentRate(uint64_t now) const {
        return Rate(now);
    }
    uint64_t UpkeepDelay(uint64_t now) const {
        return upkeepAt_ > now ? upkeepAt_ - now : 0;
    }
    bool BeginUpkeep(uint64_t now, unsigned delay, int upkeepRate = UpkeepRate) {
        if (!enabled_ || !ValidUpkeep(upkeepRate) || upkeepAt_ || rate_ != 5 || now > UINT64_MAX - delay)
            return false;
        Capture(now);
        upkeepRate_ = upkeepRate;
        managed_ = true;
        if (delay)
            upkeepAt_ = now + delay;
        else {
            rate_ = upkeepRate;
            upkeepAt_ = 0;
        }
        return true;
    }
    bool Exhaustion(uint64_t now, uint64_t& deadline) const {
        if (!started_ || (rate_ >= 0 && (!upkeepAt_ || now < upkeepAt_)))
            return false;
        const auto start = upkeepAt_ ? (std::max)(at_, upkeepAt_) : at_;
        if (now < start)
            return false;
        const auto amount = upkeepAt_ ? Accrue(amount_, rate_, start - at_) : amount_;
        const auto drain = DrainTime(amount);
        if (now - start < drain)
            return false;
        deadline = start + drain;
        return true;
    }
    bool ExpiredOnRestore() const {
        return expiredOnRestore_;
    }
    bool EndUpkeep(uint64_t now, WorldActionOutput& out) {
        if (!managed_)
            return false;
        uint64_t exhausted = 0;
        if (Exhaustion(now, exhausted)) {
            amount_ = 0;
            at_ = exhausted;
            rate_ = 5;
            upkeepAt_ = 0;
            Capture(now);
        } else {
            Capture(now);
            rate_ = 5;
            upkeepAt_ = 0;
        }
        return Emit(now, out);
    }
    bool RestoreLedger(unsigned amount, int rate, uint64_t savedUtc, uint64_t upkeepUtc, uint64_t now, uint64_t utc,
                       int upkeepRate = UpkeepRate, unsigned maximumDelay = 1350) {
        if (!enabled_ || !ValidUpkeep(upkeepRate) || amount > 50000 || (rate != 5 && rate != upkeepRate) || !savedUtc ||
            utc < savedUtc ||
            (upkeepUtc && (rate != 5 || upkeepUtc <= savedUtc || upkeepUtc - savedUtc > maximumDelay)))
            return false;
        const auto delay = upkeepUtc > utc ? upkeepUtc - utc : 0;
        if (delay && now > UINT64_MAX - delay)
            return false;
        expiredOnRestore_ = false;
        upkeepRate_ = upkeepRate;
        if (upkeepUtc && utc >= upkeepUtc) {
            amount = Accrue(amount, rate, upkeepUtc - savedUtc);
            savedUtc = upkeepUtc;
            rate = upkeepRate_;
        }
        if (rate < 0 && utc - savedUtc >= DrainTime(amount)) {
            amount = Accrue(0, 5, utc - savedUtc - DrainTime(amount));
            rate = 5;
            expiredOnRestore_ = true;
        } else
            amount = Accrue(amount, rate, utc - savedUtc);
        amount_ = amount;
        rate_ = rate;
        at_ = now;
        started_ = true;
        reported_ = false;
        managed_ = true;
        upkeepAt_ = delay ? now + delay : 0;
        return true;
    }

    // Facets share this same ledger. Their controller validates the aggregate
    // against its active own entries and settles every nominal rate transition.
    bool SetFacetRate(uint64_t now, int rate) {
        if (!enabled_ || rate < -20 || rate > 5 || (started_ && now < at_))
            return false;
        Capture(now);
        rate_ = upkeepRate_ = rate;
        upkeepAt_ = 0;
        managed_ = true;
        return true;
    }
    bool RestoreFacetLedger(unsigned amount, int rate, uint64_t now) {
        if (!enabled_ || amount > 50000 || rate < -20 || rate > 5)
            return false;
        amount_ = amount;
        rate_ = upkeepRate_ = rate;
        at_ = now;
        upkeepAt_ = 0;
        started_ = managed_ = true;
        reported_ = expiredOnRestore_ = false;
        return true;
    }

    bool Due(uint64_t now) const {
        return enabled_ && (!reported_ || Rate(now) != reportedRate_ ||
                            ((amount_ < 50000 || Rate(now) < 0) && now >= reportedAt_ && now - reportedAt_ >= 1000));
    }
    bool CanSpend(uint32_t skill, uint64_t now) const {
        unsigned cost = 0;
        return !enabled_ || (Cost(skill, cost) && Value(now) >= cost * 1000);
    }
    // Mutate a controller copy only. Commit it after the skill's storage succeeds.
    bool Spend(uint32_t skill, uint64_t now, WorldActionOutput& out) {
        if (!enabled_)
            return true;
        unsigned cost = 0;
        if (!Cost(skill, cost))
            return false;
        const unsigned available = Value(now);
        if (available < cost * 1000)
            return false;
        Capture(now);
        amount_ = available - cost * 1000;
        return Emit(now, out);
    }
    bool Advance(uint64_t now, WorldActionOutput& out) {
        if (!Due(now))
            return true;
        Capture(now);
        return Emit(now, out);
    }
};
} // namespace modern
} // namespace glide
