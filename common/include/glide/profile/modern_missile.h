#pragma once
#include "glide/profile/world_actions.h"
#include "glide/profile/missile_launch.h"
#include "glide/profile/projectile_combat.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace glide {
namespace modern {
// Shared flight, build-specific identities. Default linear speed1200 and explicit group speeds are local
// adaptations. Selected enemy contact uses the existing swept-segment policy; no terrain or homing.
inline bool HasStarterMissile(uint32_t skill) {
    return skill == 12510 || skill == 5652 || skill == 13070;
}
struct ControlledMissileGroup {
    uint32_t marker = 0, instance = 0, count = 1, holdMs = 0;
    float spreadRadians = 0;
    uint32_t appearanceSkill = 0; // Zero uses the logical skill's own appearance.
    float speed = 1200;
};
class StarterMissiles {
    struct Flight {
        uint32_t id = 0, skill = 0, target = 0;
        uint64_t due = 0, launched = 0;
        uint8_t level = 0;
        uint32_t appearanceSkill = 0;
        float speed = 1200;
        float range = 0;
        bool flying = false;
        uint32_t marker = 0, instance = 0, holdMs = 0;
        uint64_t arrives = 0;
        float angle = 0;
        bool groupLeader = true, stationary = false;
        std::array<float, 3> start{}, end{};
        combat::ProjectileContact contact;
    };
    std::vector<Flight> flights_;
    uint32_t next_ = 1;
    static uint16_t CreateId(unsigned build) {
        return build == 49284 ? 721 : build == 167409 ? 1000 : build == 204489 ? 1031 : 0;
    }
    static uint16_t OwnActor(unsigned build, uint16_t peer) {
        return build == 49284 ? peer : 57344;
    }
    static bool Point(const CatalogPosition& p, std::array<float, 3>& out) {
        out = {{p.x * 32, p.y * 32, -p.z * 32}};
        for (float v : out)
            if (!std::isfinite(v))
                return false;
        return true;
    }
    static bool Create(unsigned build, uint16_t owner, const Flight& f, const std::array<float, 3>& p, bool late,
                       Buffer& out) {
        const uint16_t id = CreateId(build);
        if (!id)
            return false;
        if (!out.U16(id) || !out.VarU32(f.id) || !out.VarU32(owner) ||
            !out.VarU32(f.appearanceSkill ? f.appearanceSkill : f.skill) || !out.VarU32(0))
            return false;
        for (float v : p)
            if (!out.F32(v))
                return false;
        // Late observers must use the supplied position, not the owner's launch bone.
        return out.VarU32(0) && out.U8(0) && out.U8(late ? 1 : 0);
    }
    static bool Move(unsigned build, const Flight& f, Buffer& out) {
        const uint16_t id = CreateId(build);
        if (!id)
            return false;
        if (!out.U16(static_cast<uint16_t>(id + 3)) || !out.VarU32(f.id) || !out.U8(2) || !out.VarU32(0) ||
            !out.VarU32(0))
            return false;
        for (float v : f.end)
            if (!out.F32(v))
                return false;
        return out.VarU32(0) && out.F32(f.speed) && out.VarU32(0) && out.U64(0) && out.U8(0) && out.U8(0);
    }
    static bool Destroy(unsigned build, const Flight& f, Buffer& out) {
        const uint16_t id = CreateId(build);
        return id && out.U16(static_cast<uint16_t>(id + 1)) && out.VarU32(f.id);
    }
    static bool SurfaceImpact(unsigned build, const Flight& f, Buffer& out) {
        // The own clients resolve tag 0x4000 and check the surface themselves. Only Long Range Shot has that tag
        // in the extracted starter missiles. This is not damage, a guaranteed explosion, or a teardown
        // notification.
        if ((build != 167409 && build != 204489) || f.skill != 12510)
            return true;
        return out.U16(static_cast<uint16_t>(CreateId(build) + 2)) && out.VarU32(f.id);
    }
    static bool TerminalView(unsigned build, const Flight& f, const std::array<float, 3>& position, Buffer& out) {
        if (build != 49284 || f.appearanceSkill != 6073)
            return true;
        // HoT creates AvMissile on the first movement notification, not Create.
        // A two-native-unit path at speed 1 avoids its zero-distance fallback;
        // flag 2 preserves the supplied destination. This is a visual adaptation.
        if (!out.U16(724) || !out.VarU32(f.id) || !out.U8(2) || !out.VarU32(0) || !out.VarU32(0))
            return false;
        return out.F32(position[0] + 2) && out.F32(position[1]) && out.F32(position[2]) && out.VarU32(0) &&
               out.F32(1) && out.VarU32(2) && out.U64(0) && out.U8(0) && out.U8(0);
    }
    // Separate transient instances let HoT's air and water spheres coexist.
    // HoT's durable effect controller reserves both of these instance IDs.
    static uint32_t MarkerBuff(uint32_t skill) {
        return skill == 5652 ? 5654 : skill == 5605 ? 5612 : 0;
    }
    static uint32_t MarkerInstance(uint32_t skill) {
        return skill == 5605 ? UINT32_MAX - 1 : UINT32_MAX;
    }
    static bool Marker(unsigned build, uint16_t owner, const Flight& f, uint64_t now, bool remove, Buffer& out) {
        const auto buff = f.marker;
        if (!buff || !f.groupLeader)
            return true;
        const uint16_t create = build == 49284 ? 497 : build == 167409 ? 713 : build == 204489 ? 735 : 0;
        if (!create)
            return false;
        if (remove)
            return out.U16(static_cast<uint16_t>(create + (build == 49284 ? 3 : 4))) && out.VarU32(owner) &&
                   out.VarU32(owner) && out.VarU32(f.instance);
        const uint64_t remaining = f.due > now ? f.due - now : 0;
        return remaining && remaining <= UINT32_MAX && out.U16(create) && out.VarU32(owner) && out.VarU32(owner) &&
               out.VarU32(buff) && out.VarU32(f.instance) && out.VarU32(static_cast<uint32_t>(remaining)) &&
               out.U8(f.level) && out.U8(1);
    }

  public:
    bool Schedule(uint32_t skill, unsigned delay, float range, uint32_t target, const WorldActionContext& context,
                  unsigned level = 80, unsigned build = 204489, const ControlledMissileGroup* group = nullptr) {
        if (!group && !HasStarterMissile(skill) && (skill != 5605 || build != 49284))
            return true;
        const bool modernSphere = (build == 167409 || build == 204489) && skill == 28827 && group &&
                                  group->marker == 28631 && group->instance == UINT32_MAX - 6 && group->count == 1 &&
                                  !group->holdMs && !group->spreadRadians && group->appearanceSkill == 5652 &&
                                  group->speed == 300;
        const bool modernBrand = (build == 167409 || build == 204489) && skill == 28692 && group && !group->marker &&
                                 !group->instance && group->count == 1 && !group->holdMs && !group->spreadRadians &&
                                 !group->appearanceSkill && group->speed == 1200;
        if (group &&
            ((build != 49284 && !modernSphere && !modernBrand) || bool(group->marker) != bool(group->instance) ||
             group->count < 1 || group->count > 3 || group->holdMs > 60000 || !std::isfinite(group->spreadRadians) ||
             group->spreadRadians < 0 || group->spreadRadians > .5f || !std::isfinite(group->speed) ||
             group->speed < 1 || group->speed > 100000 ||
             (group->appearanceSkill && !(skill == 28827 && group->appearanceSkill == 5652))))
            return false;
        const uint32_t count = group ? group->count : 1;
        const uint16_t first = build == 49284 ? 257 : 60000;
        if (!CreateId(build) || (build == 49284 && !group && !HasStarterMissile(skill) && skill != 5605) ||
            level > 80 || context.peerAgent < first || context.peerAgent > first + 253 || !std::isfinite(range) ||
            range <= 0 || range > 100000 || context.now > UINT64_MAX - delay || flights_.size() + count > 64)
            return false;
        const auto marker = group ? group->marker : MarkerBuff(skill);
        const auto instance = group ? group->instance : marker ? MarkerInstance(skill) : 0;
        if (marker)
            for (const auto& f : flights_)
                if (f.skill == skill || f.marker == marker || f.instance == instance)
                    return false;
        if (next_ > 65536 - count) {
            if (!flights_.empty())
                return false;
            next_ = 1;
        }
        for (uint32_t i = 0; i < count; ++i) {
            Flight f;
            f.id = ((static_cast<uint32_t>(context.peerAgent) - first + 1) << 16) | next_++;
            f.skill = skill;
            f.target = target;
            f.range = range;
            f.due = context.now + delay;
            f.level = static_cast<uint8_t>(level);
            f.contact.Capture(build, skill, target, context);
            f.marker = marker;
            f.instance = instance;
            f.groupLeader = i == 0;
            if (group) {
                f.holdMs = group->holdMs;
                f.angle = (float(i) - float(count - 1) / 2) * group->spreadRadians;
                f.appearanceSkill = group->appearanceSkill;
                f.speed = group->speed;
            }
            flights_.push_back(f);
        }
        return true;
    }
    bool CombatDue(uint64_t now) const {
        for (const auto& f : flights_)
            if (f.flying && f.contact.skill && now > (f.contact.sampled ? f.contact.sampledAt : f.launched))
                return true;
        return false;
    }
    // Sampling without a hit records observed time only. A hit stages its
    // complete retirement output before one durable save and returns immediately.
    int AdvanceCombat(unsigned build, uint64_t version, const WorldActionContext& c, WorldActionOutput& out) {
        for (size_t i = 0; i < flights_.size(); ++i) {
            const auto& flight = flights_[i];
            if (!flight.flying || !flight.contact.skill)
                continue;
            auto contact = flight.contact;
            WorldHealthChange enemy;
            const int hit =
                contact.Sample(build, c, flight.launched, flight.arrives, flight.start, flight.end, 39, enemy);
            if (hit < 0)
                return -1;
            if (!hit) {
                flights_[i].contact = std::move(contact);
                continue;
            }
            if (version == UINT64_MAX || !c.persistCombatHit)
                return -1;
            auto next = *this;
            auto& pending = next.flights_[i];
            WorldActionOutput prepared;
            prepared.sequences = 0;
            if (c.now < pending.due)
                pending.contact = {};
            else {
                if (!SurfaceImpact(build, pending, prepared.owned) || !Destroy(build, pending, prepared.owned) ||
                    !Marker(build, OwnActor(build, c.peerAgent), pending, c.now, true, prepared.owned) ||
                    !SurfaceImpact(build, pending, prepared.peers) || !Destroy(build, pending, prepared.peers) ||
                    !Marker(build, c.peerAgent, pending, c.now, true, prepared.peers))
                    return -1;
                next.flights_.erase(next.flights_.begin() + i);
            }
            if (!c.ReserveSequences(prepared.sequences))
                return -1;
            const auto saved = c.persistCombatHit(flight.skill, c.healthState, enemy);
            if (saved == WorldConditionCommit::RecoveryRequired)
                return -1;
            out = {};
            out.sequences = 0;
            if (saved != WorldConditionCommit::Applied)
                return 0;
            // Early body contact consumes damage while the visible flight
            // continues. No visual packet means no published effect version.
            prepared.peerEffectVersion = prepared.peers.Size() ? version + 1 : 0;
            *this = std::move(next);
            out = std::move(prepared);
            return 2;
        }
        return 1;
    }
    bool CanDetonate(uint64_t now, uint32_t skill = 5652) const {
        for (const auto& f : flights_)
            if (f.marker && f.skill == skill && f.flying && now < f.due)
                return true;
        return false;
    }
    bool Detonate(unsigned build, const WorldActionContext& context, WorldActionOutput& out, uint32_t skill = 5652,
                  uint32_t terminalAppearanceSkill = 0, uint32_t terminalLifetimeMs = 0) {
        const bool terminal = terminalAppearanceSkill || terminalLifetimeMs;
        // Reuse the own HoT detonation appearance for one local second.
        // This visual adaptation does not imply damage or an original duration.
        if (terminal && (build != 49284 || (skill != 5941 && skill != 6147) || terminalAppearanceSkill != 6073 ||
                         terminalLifetimeMs != 1000 || context.now > UINT64_MAX - terminalLifetimeMs))
            return false;
        if (!CanDetonate(context.now, skill))
            return false;
        std::vector<Flight> remaining;
        Buffer own, peers;
        for (const auto& f : flights_) {
            if (f.skill != skill) {
                remaining.push_back(f);
                continue;
            }
            if (!Destroy(build, f, own) ||
                !Marker(build, OwnActor(build, context.peerAgent), f, context.now, true, own) ||
                !Destroy(build, f, peers) || !Marker(build, context.peerAgent, f, context.now, true, peers))
                return false;
            if (terminal) {
                auto visual = f;
                auto position = f.start;
                const double fraction =
                    f.stationary ? 1.0
                    : context.now <= f.launched
                        ? 0
                        : (std::min)(1.0, double(context.now - f.launched) / double(f.arrives - f.launched));
                for (size_t i = 0; i < 3; ++i)
                    position[i] += static_cast<float>(fraction * (f.end[i] - position[i]));
                visual.start = visual.end = position;
                visual.appearanceSkill = terminalAppearanceSkill;
                visual.marker = visual.instance = 0;
                visual.holdMs = terminalLifetimeMs;
                visual.stationary = true;
                visual.launched = visual.arrives = context.now;
                visual.due = context.now + terminalLifetimeMs;
                if (!Create(build, OwnActor(build, context.peerAgent), visual, position, true, own) ||
                    !TerminalView(build, visual, position, own) ||
                    !Create(build, context.peerAgent, visual, position, true, peers) ||
                    !TerminalView(build, visual, position, peers))
                    return false;
                remaining.push_back(visual);
            }
        }
        if (!out.owned.Append(own.View()) || !out.peers.Append(peers.View()) ||
            (out.targetPeerAgent && !out.targetView.Append(peers.View())))
            return false;
        flights_ = std::move(remaining);
        return true;
    }
    void CancelPending() {
        flights_.erase(std::remove_if(flights_.begin(), flights_.end(), [](const Flight& f) { return !f.flying; }),
                       flights_.end());
    }
    template <class Available> bool Due(uint64_t now, Available available) const {
        for (const auto& f : flights_)
            if (now >= f.due || (!f.flying && !available(f.skill)) ||
                (f.holdMs && f.flying && !f.stationary && now >= f.arrives))
                return true;
        return false;
    }
    template <class Available>
    bool Advance(unsigned build, const WorldActionContext& context, Available available, WorldActionOutput& out) {
        std::vector<Flight> remaining;
        Buffer owned, peers;
        for (auto f : flights_) {
            // Another input may advance visuals before the combat Tick. Keep a
            // due hostile contact until its health decision has been settled.
            if (f.flying && f.contact.skill && context.now >= f.arrives) {
                remaining.push_back(f);
                continue;
            }
            if (!f.flying && !available(f.skill))
                continue;
            if (f.holdMs && f.flying && !f.stationary && context.now >= f.arrives && context.now < f.due) {
                // Reuse the native initial stationary state at the endpoint.
                // No fabricated zero-speed movement or predicted animation.
                if (!Destroy(build, f, owned) ||
                    !Create(build, OwnActor(build, context.peerAgent), f, f.end, true, owned) ||
                    !Destroy(build, f, peers) || !Create(build, context.peerAgent, f, f.end, true, peers))
                    return false;
                f.stationary = true;
            }
            if (context.now < f.due) {
                remaining.push_back(f);
                continue;
            }
            if (f.flying) {
                if (!SurfaceImpact(build, f, owned) || !Destroy(build, f, owned) ||
                    !Marker(build, OwnActor(build, context.peerAgent), f, context.now, true, owned) ||
                    !SurfaceImpact(build, f, peers) || !Destroy(build, f, peers) ||
                    !Marker(build, context.peerAgent, f, context.now, true, peers))
                    return false;
                continue;
            }
            if (!Point(context.position, f.start))
                return false;
            if (f.target) {
                CatalogPosition target;
                if (!f.contact.Matches(context))
                    continue;
                if (f.target == OwnActor(build, context.peerAgent) || f.target == context.peerAgent ||
                    !context.characterTargetPosition || !context.characterTargetPosition(f.target, target))
                    continue;
                if (!Point(target, f.end))
                    return false;
            } else {
                const double x = context.hasFacing ? context.facing.x : 1, y = context.hasFacing ? context.facing.y : 0;
                const double length = std::sqrt(x * x + y * y);
                if (!std::isfinite(length) || length < .001)
                    continue;
                f.end = f.start;
                f.end[0] += static_cast<float>(f.range * x / length);
                f.end[1] += static_cast<float>(f.range * y / length);
            }
            if (f.angle) {
                const double x = double(f.end[0]) - f.start[0], y = double(f.end[1]) - f.start[1];
                f.end[0] = f.start[0] + static_cast<float>(x * std::cos(f.angle) - y * std::sin(f.angle));
                f.end[1] = f.start[1] + static_cast<float>(x * std::sin(f.angle) + y * std::cos(f.angle));
            }
            double squared = 0;
            for (size_t i = 0; i < 3; ++i) {
                const double d = double(f.end[i]) - f.start[i];
                squared += d * d;
            }
            const double distance = std::sqrt(squared);
            if (!std::isfinite(distance) || distance <= 1 || distance > f.range + .01)
                continue;
            const auto duration = static_cast<uint64_t>(std::ceil(distance * 1000 / f.speed));
            if (!duration || context.now > UINT64_MAX - duration - f.holdMs)
                return false;
            RaiseWeaponMissile(f.start, f.end);
            f.launched = context.now;
            f.arrives = context.now + duration;
            f.due = f.arrives + f.holdMs;
            f.flying = true;
            if (!Create(build, OwnActor(build, context.peerAgent), f, f.start, false, owned) ||
                !Move(build, f, owned) ||
                !Marker(build, OwnActor(build, context.peerAgent), f, context.now, false, owned) ||
                !Create(build, context.peerAgent, f, f.start, false, peers) || !Move(build, f, peers) ||
                !Marker(build, context.peerAgent, f, context.now, false, peers))
                return false;
            remaining.push_back(f);
        }
        if (!out.owned.Append(owned.View()) || !out.peers.Append(peers.View()) ||
            (out.targetPeerAgent && !out.targetView.Append(peers.View())))
            return false;
        flights_ = std::move(remaining);
        return true;
    }
    bool Snapshot(unsigned build, uint16_t owner, uint64_t now, Buffer& out) const {
        for (const auto& f : flights_)
            if (f.flying && now < f.due && (f.stationary || f.arrives > f.launched)) {
                auto position = f.start;
                const double fraction =
                    f.stationary        ? 1.0
                    : now <= f.launched ? 0
                                        : (std::min)(1.0, double(now - f.launched) / double(f.arrives - f.launched));
                for (size_t i = 0; i < 3; ++i)
                    position[i] += static_cast<float>(fraction * (f.end[i] - position[i]));
                if (!Create(build, owner, f, position, true, out) || !TerminalView(build, f, position, out) ||
                    ((!f.holdMs || now < f.arrives) && !Move(build, f, out)) ||
                    !Marker(build, owner, f, now, false, out))
                    return false;
            }
        return true;
    }
    bool Cleanup(unsigned build, Buffer& out, uint16_t owner = 0) const {
        for (const auto& f : flights_)
            if (f.flying && (!Destroy(build, f, out) || (owner && !Marker(build, owner, f, 0, true, out))))
                return false;
        return true;
    }
};
} // namespace modern
} // namespace glide
