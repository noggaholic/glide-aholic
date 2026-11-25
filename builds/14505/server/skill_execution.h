#pragma once
#include "motion_state.h"
#include "world_protocol.h"
#include "../../14812/server/motion.h"
#include <vector>
#include <limits>
#include <cmath>
#include <algorithm>

namespace glide {
namespace june14505 {
struct SkillPhase {
    uint8_t index = 0;
    uint16_t duration = 0;
    uint8_t type = 0;
    std::array<float, 10> movement{};
};
enum class SkillProgress { Waiting, Emitted, Completed, Invalid };
// Owned by one world session. Plan selection, loop expansion and recharge policy
// belong to the caller. This advances an already selected, bounded phase plan.
class SkillExecution {
  public:
    explicit SkillExecution(unsigned build = 14505) : build_(build) {}
    bool Begin(const SkillStage& skill, uint32_t identity, const std::vector<SkillPhase>& phases, uint64_t now) {
        if (!WorldProtocolBuild(build_) || Active() || !skill.skill || skill.slot >= 17 || !identity ||
            (identity & 0x80000000u) || phases.empty() || phases.size() > 256 || phases.front().index)
            return false;
        uint32_t duration = 0;
        for (const auto& phase : phases) {
            if (!phase.duration)
                return false;
            if (phase.type) {
                july14812::SkillStage geometry;
                geometry.type = phase.type;
                geometry.durationMs = phase.duration;
                geometry.movement = phase.movement;
                july14812::SkillMovement move;
                if (!july14812::BuildSkillMovement(geometry, {{1, 0, 0}}, move, skill.underwater))
                    return false;
            }
            duration += phase.duration;
        }
        // Reused local scheduler bound from July, not a native protocol limit.
        if (duration > 60000 || now > (std::numeric_limits<uint64_t>::max)() - duration)
            return false;
        phases_ = phases;
        skill_ = skill;
        identity_ = identity;
        next_ = 0;
        due_ = now;
        return true;
    }
    bool Active() const {
        return !phases_.empty();
    }
    bool HasPendingPhase() const {
        return next_ < phases_.size();
    }
    uint64_t Due() const {
        return due_;
    }
    bool WithinPhases(unsigned first, unsigned outcome, uint64_t now) const {
        if (!next_ || next_ > phases_.size() || phases_[next_ - 1].index < first || phases_[next_ - 1].index >= outcome)
            return false;
        uint64_t end = due_;
        for (size_t i = next_; i < phases_.size() && phases_[i].index < outcome; ++i) {
            if (end > UINT64_MAX - phases_[i].duration)
                return false;
            end += phases_[i].duration;
        }
        return now < end;
    }
    // Close the current authored loop before its outcome; keep identity and
    // native phase indices instead of fabricating a different animation.
    bool ExitLoop(unsigned close, unsigned outcome, uint64_t now, uint32_t target) {
        if (!Active() || !next_ || next_ > phases_.size())
            return false;
        size_t finish = next_;
        while (finish < phases_.size() && phases_[finish].index != outcome)
            ++finish;
        if (finish == phases_.size())
            return false;
        size_t closing = finish;
        if (phases_[next_ - 1].index != close)
            for (size_t i = next_; i < finish; ++i)
                if (phases_[i].index == close) {
                    closing = i;
                    break;
                }
        std::vector<SkillPhase> pending(phases_.begin(), phases_.begin() + next_);
        if (closing < finish)
            pending.push_back(phases_[closing]);
        pending.insert(pending.end(), phases_.begin() + finish, phases_.end());
        phases_ = std::move(pending);
        due_ = now;
        skill_.target = target;
        return true;
    }
    bool PhaseDue(unsigned index, uint64_t now) const {
        return HasPendingPhase() && phases_[next_].index == index && now >= due_;
    }
    bool Snapshot(uint64_t now, const CharacterAction& pose, uint16_t agent, uint32_t sequence, Buffer& out) const {
        if (!Active() || !next_)
            return true;
        SkillStage stage = skill_;
        stage.index = phases_[next_ - 1].index;
        stage.initial = true;
        stage.final = next_ == phases_.size();
        auto action = emittedAction_;
        action.pose = pose.pose;
        const uint64_t remaining = due_ > now ? due_ - now : 1;
        action.words[2] = static_cast<uint16_t>((std::min)(remaining, uint64_t(phases_[next_ - 1].duration)));
        return EncodeSkillStageForBuild(build_, stage, action, agent, sequence, out);
    }
    bool Cancel(const CharacterAction& pose, uint16_t ownedAgent, uint16_t peerAgent, uint32_t sequence, Buffer& owned,
                Buffer& peer) {
        if (!Active() || &owned == &peer)
            return false;
        auto action = pose;
        action.identity = identity_;
        Buffer ownNext, peerNext;
        if (!EncodeSkillEndForBuild(build_, action, ownedAgent, sequence, ownNext) ||
            !EncodeSkillEndForBuild(build_, action, peerAgent, sequence, peerNext))
            return false;
        owned = std::move(ownNext);
        peer = std::move(peerNext);
        Reset();
        return true;
    }
    void Reset() {
        phases_.clear();
        next_ = 0;
        identity_ = 0;
        due_ = 0;
        skill_ = SkillStage{};
    }
    SkillProgress Advance(uint64_t now, const CharacterAction& pose, uint16_t ownedAgent, uint16_t peerAgent,
                          uint32_t sequence, Buffer& owned, Buffer& peer,
                          const std::array<float, 3>& facing = {{1, 0, 0}},
                          double targetDistance = std::numeric_limits<double>::infinity()) {
        if (!Active() || now < due_)
            return SkillProgress::Waiting;
        if (next_ == phases_.size()) {
            Reset();
            return SkillProgress::Completed;
        }
        const auto& phase = phases_[next_];
        if (&owned == &peer || now > (std::numeric_limits<uint64_t>::max)() - phase.duration)
            return SkillProgress::Invalid;
        SkillStage stage = skill_;
        stage.index = phase.index;
        stage.initial = next_ == 0;
        stage.final = next_ + 1 == phases_.size();
        CharacterAction action = pose;
        action.identity = identity_;
        action.words[2] = phase.duration;
        if (std::isnan(targetDistance) || targetDistance < 0)
            return SkillProgress::Invalid;
        if (phase.type && targetDistance > 0.001) {
            july14812::SkillStage geometry;
            geometry.type = phase.type;
            geometry.durationMs = phase.duration;
            geometry.movement = phase.movement;
            july14812::SkillMovement move;
            if (!july14812::BuildSkillMovement(geometry, facing, move, skill_.underwater))
                return SkillProgress::Invalid;
            // Only forward motion approaches the target. Backward evades retain their authored speed.
            const double forward =
                move.direction[0] * facing[0] + move.direction[1] * facing[1] + move.direction[2] * facing[2];
            if (forward > 0 && std::isfinite(targetDistance))
                move.speed = (std::min)(move.speed, static_cast<float>(targetDistance * 1000 / phase.duration));
            action.moving = true;
            action.direction = move.direction;
            action.movementSpeed = move.speed;
            const double yaw = std::atan2(move.facing[1], move.facing[0]);
            const double pitch = std::atan2(move.facing[2], std::hypot(move.facing[0], move.facing[1]));
            action.words[0] =
                static_cast<uint16_t>(static_cast<int>(std::round(pitch * 32766 / 3.14159265358979323846)));
            action.words[1] = static_cast<uint16_t>(static_cast<int>(std::round(yaw * 32766 / 3.14159265358979323846)));
        }
        Buffer ownNext, peerNext;
        if (!EncodeSkillStageForBuild(build_, stage, action, ownedAgent, sequence, ownNext) ||
            !EncodeSkillStageForBuild(build_, stage, action, peerAgent, sequence, peerNext))
            return SkillProgress::Invalid;
        owned = std::move(ownNext);
        peer = std::move(peerNext);
        emittedAction_ = action;
        ++next_;
        // A delayed worker does not burst all overdue phases into the client.
        // Each published phase receives its full original duration, as in July.
        due_ = now + phase.duration;
        return SkillProgress::Emitted;
    }

  private:
    unsigned build_;
    SkillStage skill_;
    std::vector<SkillPhase> phases_;
    size_t next_ = 0;
    uint32_t identity_ = 0;
    uint64_t due_ = 0;
    CharacterAction emittedAction_;
};
} // namespace june14505
} // namespace glide
