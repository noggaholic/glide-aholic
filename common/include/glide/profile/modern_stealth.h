#pragma once
#include "glide/profile/world_actions.h"
#include <cmath>

namespace glide {
namespace modern {
// Contact is estimated with the existing linear projectile speed1200. It is not a damage/collision calculation.
// Three-second PvE windows are explicit.
struct StealthRules {
    enum : unsigned {
        Source = 13070,
        SelfSource = 13027,
        SelfActivationMs = 1350,
        Attack = 13125,
        Buff = 13017,
        Revealed = 890,
        ReleaseMs = 400,
        AttackHitMs = 476,
        Range = 900,
        Speed = 1200,
        DurationMs = 3000,
        Instance = 0xfffffff8
    };
};
template <unsigned Build> class Stealth {
    static_assert(Build == 167409 || Build == 204489, "Unsupported stealth dialect");
    uint32_t buff_ = 0, grantTarget_ = 0, grantSkill_ = 0, attackTarget_ = 0, attackSkill_ = 0;
    uint64_t expires_ = 0, grantDue_ = 0, attackDue_ = 0;
    unsigned level_ = 0;
    float attackRange_ = 0;
    static bool Distance(const WorldActionContext& c, uint32_t target, float range, double& distance) {
        CatalogPosition p;
        if (!target || target == 57344 || target == c.peerAgent || !std::isfinite(range) || range <= 0 ||
            !c.isVisibleCharacterTarget || !c.isVisibleCharacterTarget(target) || !c.characterTargetPosition ||
            !c.characterTargetPosition(target, p))
            return false;
        const double x = double(p.x) - c.position.x, y = double(p.y) - c.position.y, z = double(p.z) - c.position.z;
        distance = std::sqrt(x * x + y * y + z * z) * 32;
        return std::isfinite(distance) && distance <= range;
    }
    static bool Remove(uint16_t actor, Buffer& out) {
        return out.U16(Build == 167409 ? 717 : 739) && out.VarU32(actor) && out.VarU32(actor) &&
               out.VarU32(StealthRules::Instance);
    }
    bool Create(uint16_t actor, uint64_t now, Buffer& out) const {
        return actor && level_ <= 80 && expires_ > now && expires_ - now <= StealthRules::DurationMs &&
               out.U16(Build == 167409 ? 713 : 735) && out.VarU32(actor) && out.VarU32(actor) && out.VarU32(buff_) &&
               out.VarU32(StealthRules::Instance) && out.VarU32(static_cast<uint32_t>(expires_ - now)) &&
               out.U8(static_cast<uint8_t>(level_)) && out.U8(1);
    }
    bool Set(uint32_t buff, const WorldActionContext& c, WorldActionOutput& out, uint64_t deadline = 0) {
        if (buff) {
            if (!deadline) {
                if (c.now > UINT64_MAX - StealthRules::DurationMs)
                    return false;
                deadline = c.now + StealthRules::DurationMs;
            }
            if (deadline <= c.now || deadline - c.now > StealthRules::DurationMs)
                return false;
        }
        Buffer owned, remote;
        if (buff_ && (!Remove(57344, owned) || !Remove(c.peerAgent, remote)))
            return false;
        buff_ = buff;
        expires_ = 0;
        if (buff) {
            expires_ = deadline;
            if (!Create(57344, c.now, owned) || !Create(c.peerAgent, c.now, remote))
                return false;
        }
        return out.owned.Append(owned.View()) && out.peers.Append(remote.View()) &&
               (!out.targetPeerAgent || out.targetView.Append(remote.View()));
    }

  public:
    bool Active(uint64_t now) const {
        return buff_ == StealthRules::Buff && now < expires_;
    }
    bool Due() const {
        return buff_ || grantSkill_ || attackTarget_;
    }
    // Call on a disposable receiver copy until its durable reveal commits.
    // Preserve already accepted attacks and grants resolving after reveal expires.
    bool Suppress(const WorldActionContext& c, WorldActionOutput& out) {
        return buff_ != StealthRules::Buff || Set(0, c, out);
    }
    void CancelPending() {
        grantTarget_ = grantSkill_ = attackTarget_ = 0;
    }
    // Own modern13027 facts/timing supply a personal source. It has no contact,
    // hostile target or artificial range. This transient cast agenda is never
    // reconstructed by a snapshot/restore; the wrapper schedules only acceptance.
    bool ScheduleSelf(unsigned activation, unsigned level, const WorldActionContext& c) {
        if (activation != StealthRules::SelfActivationMs || level > 80 ||
            c.now > UINT64_MAX - StealthRules::SelfActivationMs - StealthRules::DurationMs)
            return false;
        level_ = level;
        if (!(buff_ == StealthRules::Revealed && c.now < expires_)) {
            grantSkill_ = StealthRules::SelfSource;
            grantTarget_ = 0;
            grantDue_ = c.now + StealthRules::SelfActivationMs;
        }
        return true;
    }
    bool Use(uint32_t skill, uint32_t target, unsigned activation, float range, unsigned level,
             const WorldActionContext& c) {
        if (skill == StealthRules::SelfSource)
            return ScheduleSelf(activation, level, c);
        if (level > 80)
            return false;
        level_ = level;
        double distance = 0;
        if (!Distance(c, target, range, distance))
            return true; // A miss grants/consumes nothing.
        unsigned delay = skill == StealthRules::Attack ? StealthRules::AttackHitMs : activation;
        if (skill == StealthRules::Source)
            delay = StealthRules::ReleaseMs + static_cast<unsigned>(std::ceil(distance * 1000 / StealthRules::Speed));
        if (c.now > UINT64_MAX - delay)
            return false;
        if (Active(c.now)) {
            attackTarget_ = target;
            attackSkill_ = skill;
            attackDue_ = c.now + delay;
            attackRange_ = range;
        }
        if (skill == StealthRules::Source && !(buff_ == StealthRules::Revealed && c.now < expires_)) {
            grantTarget_ = target;
            grantSkill_ = StealthRules::Source;
            grantDue_ = c.now + delay;
        }
        return true;
    }
    template <class Available> bool Advance(const WorldActionContext& c, Available available, WorldActionOutput& out) {
        double distance = 0;
        if (c.revealed && !Suppress(c, out))
            return false;
        if (buff_ && c.now >= expires_ && !Set(0, c, out))
            return false;
        if (attackTarget_) {
            if (!available(attackSkill_) || !Distance(c, attackTarget_, attackRange_, distance))
                attackTarget_ = 0;
            else if (c.now >= attackDue_) {
                attackTarget_ = 0;
                if (Active(c.now) && !Set(StealthRules::Revealed, c, out))
                    return false;
            }
        }
        if (grantSkill_) {
            const bool personal = grantSkill_ == StealthRules::SelfSource;
            if (!available(grantSkill_) || (!personal && !Distance(c, grantTarget_, StealthRules::Range, distance)))
                grantTarget_ = grantSkill_ = 0;
            else if (c.now >= grantDue_) {
                grantTarget_ = grantSkill_ = 0;
                const auto deadline = personal ? grantDue_ + StealthRules::DurationMs : 0;
                if ((!personal || c.now < deadline) && !c.revealed &&
                    !(buff_ == StealthRules::Revealed && c.now < expires_) &&
                    !Set(StealthRules::Buff, c, out, deadline))
                    return false;
            }
        }
        return true;
    }
    bool Snapshot(uint16_t actor, uint64_t now, Buffer& out) const {
        return !buff_ || now >= expires_ || Create(actor, now, out);
    }
    bool Cleanup(uint16_t actor, Buffer& out) const {
        return !buff_ || Remove(actor, out);
    }
};
} // namespace modern
} // namespace glide
