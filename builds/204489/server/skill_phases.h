#pragma once
#include "../../14812/server/motion.h"
#include "glide/files.h"
#include "glide/profile/world_actions.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>

namespace glide {
namespace modern {
using SkillStage = july14812::SkillStage;
using SkillMovement = july14812::SkillMovement;
using SkillPhases = std::vector<SkillStage>;

inline bool SkillTargetInRange(const WorldActionContext& context, uint32_t target, float minimum, float maximum) {
    if (!target || target == 57344)
        return true;
    CatalogPosition point;
    if (!context.characterTargetPosition || !context.characterTargetPosition(target, point))
        return false;
    const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y,
                 z = double(point.z) - context.position.z;
    const double distance = std::sqrt(x * x + y * y + z * z) * 32;
    return std::isfinite(distance) && std::isfinite(minimum) && std::isfinite(maximum) && minimum >= 0 &&
           maximum >= minimum && distance >= minimum && distance <= maximum;
}

// Each build publishes these rows from its own archive, checked against its
// original process captures. Geometry reuses the July implementation.
inline bool LoadSkillPhases(const std::wstring& file, std::map<uint32_t, SkillPhases>& out) {
    Buffer bytes;
    if (!ReadFileBytes(file, 65536, bytes))
        return false;
    std::istringstream document(std::string(reinterpret_cast<const char*>(bytes.Data()), bytes.Size()));
    std::map<uint32_t, SkillPhases> result;
    std::string line;
    while (std::getline(document, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        std::istringstream row(line);
        uint32_t skill = 0;
        unsigned index = 0, type = 0, duration = 0, flags = 0;
        std::string extra;
        if (!(row >> skill >> index >> type >> duration >> flags) || !skill || skill >= 0x400000 || index >= 32 ||
            type > 1 || !duration || duration > 65535)
            return false;
        auto& phases = result[skill];
        if (index != phases.size())
            return false;
        SkillStage stage;
        stage.index = static_cast<uint8_t>(index);
        stage.type = static_cast<uint8_t>(type);
        stage.durationMs = static_cast<uint16_t>(duration);
        if (type) {
            for (float& value : stage.movement)
                if (!(row >> value) || !std::isfinite(value))
                    return false;
            SkillMovement movement;
            if (!july14812::BuildSkillMovement(stage, {{1, 0, 0}}, movement))
                return false;
            if (movement.speed > static_cast<float>(UINT32_MAX))
                return false;
        }
        if (row >> extra)
            return false;
        phases.push_back(stage);
    }
    if (result.empty())
        return false;
    out = std::move(result);
    return true;
}
inline bool SkillEnd(uint16_t actor, uint32_t sequence, uint32_t identity, Buffer& out) {
    return out.U16(84) && out.U16(actor) && out.VarU32(sequence) && out.U8(0x10) && out.VarU32(identity) && out.U8(1) &&
           out.U8(69);
}
inline bool SkillPhase(uint16_t actor, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                       uint32_t target, const SkillStage& stage, bool initial, bool final, const SkillMovement* move,
                       Buffer& out, bool underwater = false) {
    if (!actor || !sequence || !stage.durationMs)
        return false;
    Buffer result;
    if (!result.U16(move ? 57 : 34) || !result.U16(actor) || !result.VarU32(sequence) || !result.U8(0x10) ||
        !result.VarU32(identity) || !result.U8(initial ? 13 : 4) || !result.U8(120) || !result.U8(initial ? 0 : 4))
        return false;
    if (initial) {
        if (!result.U32(skill) || !result.U8(slot) || !result.U8(stage.index) || !result.U32(target) ||
            !result.U8(underwater ? 3 : 1))
            return false;
    } else if (!result.U8(stage.index) || !result.U8(3))
        return false;
    if (move) {
        // S57: no replacement position; direction, facing, optional speed/time.
        // Native speed and duration are VarU32, not floats or fixed U16.
        if (!std::isfinite(move->speed) || move->speed < 1 || move->speed >= 4294967296.0 || !result.U8(0))
            return false;
        for (float value : move->direction)
            if (!std::isfinite(value) || !result.F32(value))
                return false;
        for (float value : move->facing)
            if (!std::isfinite(value) || !result.F32(value))
                return false;
        if (!result.U8(1) || !result.VarU32(static_cast<uint32_t>(move->speed)) || !result.VarU32(stage.durationMs) ||
            !result.U8(0))
            return false;
    } else if (!result.U16(stage.durationMs))
        return false;
    if (final && !SkillEnd(actor, sequence, identity, result))
        return false;
    return out.Append(result.View());
}
struct PhasedSkill {
    SkillPhases phases;
    uint32_t skill = 0, identity = 0, target = 0;
    uint8_t slot = 0;
    size_t index = 0;
    uint64_t endsAt = 0;
    SkillMovement emittedMovement;
    bool emitted = false, emittedMoving = false, underwater = false;
    float minimumRange = 0, maximumRange = -1;
    bool Active() const {
        return !phases.empty();
    }
    bool Due(uint64_t now) const {
        return Active() && now >= endsAt;
    }
    bool InRange(const WorldActionContext& context) const {
        return maximumRange < 0 || SkillTargetInRange(context, target, minimumRange, maximumRange);
    }
    bool Begin(const SkillPhases& source, unsigned duration, uint32_t id, uint8_t requestedSlot, uint32_t order,
               uint32_t requestedTarget, bool water = false, float minimum = 0, float maximum = -1) {
        if (!duration || source.empty())
            return false;
        PhasedSkill next;
        next.skill = id;
        next.slot = requestedSlot;
        next.identity = order;
        next.target = requestedTarget;
        next.underwater = water;
        next.minimumRange = minimum;
        next.maximumRange = maximum;
        unsigned remaining = duration;
        for (auto phase : source) {
            if (!remaining)
                break;
            phase.durationMs = static_cast<uint16_t>((std::min)(remaining, static_cast<unsigned>(phase.durationMs)));
            remaining -= phase.durationMs;
            next.phases.push_back(phase);
        }
        if (remaining)
            return false;
        *this = std::move(next);
        return true;
    }
    bool Emit(const WorldActionContext& context, uint32_t sequence, WorldActionOutput& out, bool initial) {
        if (!Active() || index >= phases.size() || !InRange(context) ||
            context.now > UINT64_MAX - phases[index].durationMs)
            return false;
        SkillStage stage = phases[index];
        SkillMovement move;
        bool moving = stage.type != 0;
        std::array<float, 3> facing = context.hasFacing
                                          ? std::array<float, 3>{{context.facing.x, context.facing.y, context.facing.z}}
                                          : std::array<float, 3>{{1, 0, 0}};
        double distance = -1;
        if (target == 57344 && moving)
            return false;
        if (target && target != 57344) {
            CatalogPosition point;
            if (!context.characterTargetPosition || !context.characterTargetPosition(target, point))
                return false;
            const double x = double(point.x) - context.position.x, y = double(point.y) - context.position.y;
            const double z = underwater ? double(point.z) - context.position.z : 0;
            distance = std::sqrt(x * x + y * y + z * z) * 32;
            if (!std::isfinite(distance))
                return false;
            if (distance > 0.001)
                facing = {{static_cast<float>(x), static_cast<float>(y), static_cast<float>(z)}};
        }
        if (moving) {
            if (!july14812::BuildSkillMovement(stage, facing, move, underwater))
                return false;
            // Stop at a target instead of travelling through it. Backward and
            // sideways authored movement keeps its own direction and speed.
            const double forward =
                move.direction[0] * facing[0] + move.direction[1] * facing[1] + move.direction[2] * facing[2];
            if (distance >= 0 && forward > 0)
                move.speed = static_cast<float>((std::min)(double(move.speed), distance * 1000 / stage.durationMs));
            if (move.speed < 1)
                moving = false;
        }
        const bool final = index + 1 == phases.size();
        if (!SkillPhase(57344, sequence, identity, skill, slot, target, stage, initial, final, moving ? &move : nullptr,
                        out.owned, underwater) ||
            !SkillPhase(context.peerAgent, sequence, identity, skill, slot,
                        target == 57344 ? context.peerAgent : target, stage, initial, final, moving ? &move : nullptr,
                        out.peers, underwater))
            return false;
        if (target && target != 57344) {
            out.targetPeerAgent = static_cast<uint16_t>(target);
            if (!SkillPhase(context.peerAgent, sequence, identity, skill, slot, 57344, stage, initial, final,
                            moving ? &move : nullptr, out.targetView, underwater))
                return false;
        }
        endsAt = context.now + stage.durationMs;
        emittedMovement = move;
        emittedMoving = moving;
        emitted = true;
        return true;
    }
    bool Snapshot(const WorldActionContext& context, Buffer& out) const {
        if (!Active() || !emitted || !InRange(context))
            return true;
        if (index >= phases.size())
            return false;
        auto stage = phases[index];
        // Preserve the emitted direction/speed. Recomputing toward a target
        // would make the arriving observer see a different trajectory.
        const uint64_t remaining = endsAt > context.now ? endsAt - context.now : 1;
        stage.durationMs = static_cast<uint16_t>((std::min)(remaining, uint64_t(stage.durationMs)));
        const uint32_t visibleTarget =
            target == 57344 ? context.peerAgent : (target && target == context.observerAgent ? 57344 : target);
        return SkillPhase(context.peerAgent, context.sequence, identity, skill, slot, visibleTarget, stage, true,
                          index + 1 == phases.size(), emittedMoving ? &emittedMovement : nullptr, out, underwater);
    }
    bool Advance(const WorldActionContext& context, WorldActionOutput& out) {
        if (!Due(context.now))
            return true;
        if (index + 1 == phases.size()) {
            *this = PhasedSkill{};
            return true;
        }
        if (!context.sequence || context.sequence == UINT32_MAX)
            return false;
        // A departed target cancels the motion using a short stationary step.
        if (target && target != 57344 &&
            (!context.isVisibleCharacterTarget || !context.isVisibleCharacterTarget(target) || !InRange(context))) {
            SkillStage stop = phases[index];
            stop.durationMs = 1;
            if (!SkillPhase(57344, context.sequence, identity, skill, slot, 0, stop, false, true, nullptr, out.owned) ||
                !SkillPhase(context.peerAgent, context.sequence, identity, skill, slot, 0, stop, false, true, nullptr,
                            out.peers))
                return false;
            *this = PhasedSkill{};
            out.sequences = 1;
            return true;
        }
        ++index;
        if (!Emit(context, context.sequence, out, false))
            return false;
        out.sequences = 1;
        return true;
    }
};
} // namespace modern
} // namespace glide
