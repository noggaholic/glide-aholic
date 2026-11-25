#include "motion.h"
#include "glide/profile/beta_missile.h"
#include <cmath>
#include <cstring>
#include <utility>
#include <limits>
#include <algorithm>
namespace glide {
namespace july14812 {
namespace {
bool Take(Reader& r, void* out, size_t n) {
    ByteView bytes;
    if (!r.Bytes(n, bytes))
        return false;
    std::memcpy(out, bytes.data, n);
    return true;
}
template <class T> bool Take(Reader& r, T& out) {
    return Take(r, &out, sizeof(out));
}
bool Finite(const float* values, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!std::isfinite(values[i]))
            return false;
    return true;
}
bool OptionalFinite(const float* values) {
    for (size_t i = 0; i < 4; ++i) {
        uint32_t bits;
        std::memcpy(&bits, values + i, 4);
        if ((bits & 0x7f800000u) == 0x7f800000u && bits != 0x7f800000u)
            return false;
    }
    return true;
}
class Writer {
    Buffer value_;
    bool ok_ = true;

  public:
    void Word(uint16_t v) {
        ok_ = ok_ && value_.U16(v);
    }
    void Var(uint32_t v) {
        ok_ = ok_ && value_.VarU32(v);
    }
    void Byte(uint8_t v) {
        ok_ = ok_ && value_.U8(v);
    }
    void Float(float v) {
        ok_ = ok_ && value_.F32(v);
    }
    void Raw(const void* data, size_t n) {
        ok_ = ok_ && value_.Append(ByteView(data, n));
    }
    bool Finish(Buffer& out) {
        if (!ok_)
            return false;
        out = std::move(value_);
        return true;
    }
};
} // namespace
#define JULY_READ(value)                                                                                               \
    do {                                                                                                               \
        if (!Take(reader, value))                                                                                      \
            return DecodeStatus::Incomplete;                                                                           \
    } while (0)
#define JULY_VAR(value)                                                                                                \
    do {                                                                                                               \
        const auto status = reader.VarU32(value);                                                                      \
        if (status != DecodeStatus::Complete)                                                                          \
            return status;                                                                                             \
    } while (0)
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out, size_t& consumed) {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    MotionMessage next;
    JULY_READ(next.id);
    if (next.id < 3 || next.id > 16)
        return DecodeStatus::Unknown;
    if (next.id == 13) {
        JULY_VAR(next.sequence);
        JULY_VAR(next.tick);
        if (!Take(reader, next.position, 12))
            return DecodeStatus::Incomplete;
        JULY_VAR(next.token);
        JULY_READ(next.state);
        JULY_READ(next.mode);
        if (!Finite(next.position, 3))
            return DecodeStatus::Malformed;
    } else if (next.id == 12) {
        JULY_READ(next.scheduleDelay);
    } else {
        JULY_READ(next.agent);
        if (!next.agent)
            return DecodeStatus::Malformed;
        JULY_VAR(next.sequence);
        JULY_READ(next.state);
        if (next.id == 11) {
            JULY_READ(next.mode);
            JULY_READ(next.amount);
            if (!Finite(&next.amount, 1))
                return DecodeStatus::Malformed;
        } else if (next.id == 16) {
            JULY_READ(next.position);
            if (!Finite(next.position, 4))
                return DecodeStatus::Malformed;
        } else if (next.id != 3) {
            if (next.id != 4) {
                JULY_VAR(next.control);
                JULY_READ(next.inputSize);
                if (next.inputSize > 80)
                    return DecodeStatus::Malformed;
                if (!Take(reader, next.input, next.inputSize))
                    return DecodeStatus::Incomplete;
            }
            switch (next.id) {
            case 5:
                JULY_READ(next.words[0]);
                break;
            case 14:
                JULY_READ(next.words[0]);
                JULY_READ(next.words[1]);
                break;
            case 15:
                break;
            case 9:
                JULY_READ(next.amount);
                JULY_READ(next.words[0]);
                JULY_READ(next.words[1]);
                if (!Finite(&next.amount, 1))
                    return DecodeStatus::Malformed;
                break;
            case 10:
                JULY_READ(next.position);
                JULY_READ(next.words[0]);
                JULY_READ(next.words[1]);
                JULY_READ(next.mode);
                if (!Finite(next.position, 4))
                    return DecodeStatus::Malformed;
                break;
            case 4:
            case 6:
            case 7:
            case 8:
                JULY_READ(next.position);
                JULY_VAR(next.token);
                JULY_READ(next.vector);
                if (!Finite(next.position, 4) || !Finite(next.vector, 4))
                    return DecodeStatus::Malformed;
                if (next.id == 6) {
                    JULY_READ(next.words[0]);
                    JULY_READ(next.words[1]);
                    JULY_READ(next.words[2]);
                } else if (next.id == 8) {
                    JULY_READ(next.secondary);
                    JULY_READ(next.words[0]);
                    JULY_READ(next.words[1]);
                    JULY_READ(next.hasOptional);
                    if (!Finite(next.secondary, 3) || next.hasOptional > 1)
                        return DecodeStatus::Malformed;
                    if (next.hasOptional) {
                        JULY_READ(next.optionalValue);
                        JULY_READ(next.optionalWord);
                        JULY_READ(next.optionalVector);
                        JULY_READ(next.optionalTail);
                        if (!Finite(&next.optionalValue, 1) || !Finite(&next.optionalTail, 1) ||
                            !OptionalFinite(next.optionalVector))
                            return DecodeStatus::Malformed;
                    }
                }
                break;
            default:
                return DecodeStatus::Malformed;
            }
        }
    }
    out = next;
    consumed = reader.Position();
    return DecodeStatus::Complete;
}
#undef JULY_READ
#undef JULY_VAR
bool EncodeMotion(const MotionMessage& m, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!agent)
        return false;
    uint16_t id = 0;
    switch (m.id) {
    case 3:
        id = 8;
        break;
    case 4:
        id = 9;
        break;
    case 5:
        id = 10;
        break;
    case 6:
        id = 11;
        break;
    case 7:
        id = 25;
        break;
    case 8:
        id = 27;
        break;
    case 9:
        id = 31;
        break;
    case 10:
        id = 34;
        break;
    case 11:
        id = 39;
        break;
    case 14:
        id = 44;
        break;
    case 15:
        id = 48;
        break;
    case 16:
        id = 49;
        break;
    default:
        return false;
    }
    const bool hasInput = (m.id >= 5 && m.id <= 10) || m.id == 14 || m.id == 15;
    const bool hasVector = m.id == 4 || (m.id >= 6 && m.id <= 8), hasPosition = hasVector || m.id == 10 || m.id == 16;
    if ((hasPosition && !Finite(m.position, 4)) || (hasVector && !Finite(m.vector, 4)) ||
        (hasInput && m.inputSize > 80) || ((m.id == 9 || m.id == 11) && !Finite(&m.amount, 1)))
        return false;
    if (m.id == 8 && (!Finite(m.secondary, 3) || m.hasOptional > 1 ||
                      (m.hasOptional && (!Finite(&m.optionalValue, 1) || !Finite(&m.optionalTail, 1) ||
                                         !OptionalFinite(m.optionalVector)))))
        return false;
    Writer w;
    w.Word(id);
    w.Word(agent);
    w.Var(sequence);
    w.Byte(m.state);
    if (hasInput) {
        w.Var(m.control);
        w.Byte(m.inputSize);
        w.Raw(m.input, m.inputSize);
    }
    if (hasPosition)
        w.Raw(m.position, 16);
    if (hasVector) {
        w.Var(m.token);
        w.Raw(m.vector, 16);
    }
    if (m.id == 5)
        w.Word(m.words[0]);
    if (m.id == 6) {
        w.Word(m.words[0]);
        w.Word(m.words[1]);
        w.Word(m.words[2]);
    }
    if (m.id == 9)
        w.Float(m.amount);
    if (m.id == 9 || m.id == 10 || m.id == 14) {
        w.Word(m.words[0]);
        w.Word(m.words[1]);
    }
    if (m.id == 10 || m.id == 11)
        w.Byte(m.mode);
    if (m.id == 11)
        w.Float(m.amount);
    if (m.id == 8) {
        w.Raw(m.secondary, 12);
        w.Word(m.words[0]);
        w.Word(m.words[1]);
        w.Byte(m.hasOptional);
        if (m.hasOptional) {
            w.Float(m.optionalValue);
            w.Word(m.optionalWord);
            w.Raw(m.optionalVector, 16);
            w.Float(m.optionalTail);
        }
    }
    return w.Finish(out);
}
bool RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out) {
    MotionMessage message;
    size_t consumed = 0;
    return DecodeMotion(input, message, consumed) == DecodeStatus::Complete && consumed == input.size &&
           EncodeMotion(message, agent, sequence, out);
}
bool ValidSkillPlan(const SkillPlan& plan, unsigned slot) {
    if (!plan.skill || slot >= 17 || plan.slot != slot || plan.count > 32)
        return false;
    if (plan.instant)
        return plan.count == 0;
    if (!plan.count || plan.stages[0].index)
        return false;
    unsigned total = 0;
    for (unsigned i = 0; i < plan.count; ++i) {
        const auto& stage = plan.stages[i];
        if (!stage.durationMs || stage.index >= 32 || stage.type > 1)
            return false;
        if (stage.type) {
            SkillMovement move;
            if (!BuildSkillMovement(stage, {{1, 0, 0}}, move))
                return false;
        }
        total += stage.durationMs;
    }
    return total <= 60000;
}
bool BuildSkillMovement(const SkillStage& stage, const std::array<float, 3>& facing, SkillMovement& out,
                        bool underwater) {
    if (stage.type != 1 || !stage.durationMs || !Finite(facing.data(), 3) || !Finite(stage.movement.data(), 10))
        return false;
    // Current starter phases author equal start/end velocities. Variable speed
    // needs its native curve; never replace that curve with an average.
    if (stage.movement[8] <= 0 || stage.movement[8] != stage.movement[9])
        return false;
    const double horizontal = std::hypot(double(facing[0]), double(facing[1]));
    const double length = underwater ? std::hypot(horizontal, double(facing[2])) : horizontal;
    if (length < 0.0001)
        return false;
    // Compose the authored local rotation with the actor's yaw and pitch.
    // A vertical direction has no yaw; use zero for that otherwise undefined axis.
    const double cy = horizontal > 0.0001 ? facing[0] / horizontal : 1;
    const double sy = horizontal > 0.0001 ? facing[1] / horizontal : 0;
    const double cp = underwater ? horizontal / length : 1, sp = underwater ? facing[2] / length : 0;
    SkillMovement move;
    move.speed = stage.movement[8];
    for (unsigned offset : {0u, 4u}) {
        const float* q = stage.movement.data() + offset;
        const double norm = double(q[0]) * q[0] + double(q[1]) * q[1] + double(q[2]) * q[2] + double(q[3]) * q[3];
        if (std::fabs(norm - 1) > 0.001)
            return false;
        const double x = 1 - 2 * (double(q[1]) * q[1] + double(q[2]) * q[2]);
        const double y = 2 * (double(q[0]) * q[1] + double(q[3]) * q[2]);
        const double z = 2 * (double(q[0]) * q[2] - double(q[3]) * q[1]);
        auto& result = offset ? move.facing : move.direction;
        result = {{static_cast<float>(cy * (cp * x - sp * z) - sy * y),
                   static_cast<float>(sy * (cp * x - sp * z) + cy * y), static_cast<float>(sp * x + cp * z)}};
    }
    out = move;
    return true;
}
bool SkillStageReply(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                     uint32_t target, uint8_t stage, bool initial, bool final, uint16_t durationMs,
                     const std::array<float, 3>& position, Buffer& out, uint8_t state, const SkillMovement* movement,
                     bool underwater) {
    if ((state != 0x43 && state != 0x53) || !agent || !identity || (identity & 0x80000000u) || !skill || slot >= 17 ||
        !durationMs || !Finite(position.data(), 3) || stage >= 32)
        return false;
    MotionMessage action;
    action.id = 6;
    action.agent = agent;
    action.state = state;
    action.control = identity;
    action.inputSize = initial ? 13 : 4;
    action.input[0] = 0x22;
    if (initial) {
        std::memcpy(action.input + 2, &skill, 4);
        action.input[6] = slot;
        action.input[7] = stage;
        std::memcpy(action.input + 8, &target, 4);
        action.input[12] = underwater ? 3 : 1;
    } else {
        action.input[1] = 4;
        action.input[2] = stage;
        action.input[3] = 3;
    }
    std::memcpy(action.position, position.data(), 12);
    action.words[2] = durationMs;
    if (movement) {
        if (!Finite(movement->direction.data(), 3) || !Finite(movement->facing.data(), 3) ||
            !std::isfinite(movement->speed) || movement->speed <= 0)
            return false;
        action.id = 8;
        action.hasOptional = 1;
        action.optionalValue = movement->speed;
        action.optionalWord = durationMs;
        std::copy(movement->direction.begin(), movement->direction.end(), action.secondary);
        const double yaw = std::atan2(movement->facing[1], movement->facing[0]);
        const double pitch = std::atan2(movement->facing[2], std::hypot(movement->facing[0], movement->facing[1]));
        action.words[0] = static_cast<uint16_t>(static_cast<int>(std::round(pitch * 32766 / 3.14159265358979323846)));
        action.words[1] = static_cast<uint16_t>(static_cast<int>(std::round(yaw * 32766 / 3.14159265358979323846)));
        for (unsigned i = 0; i < 3; ++i)
            action.optionalVector[i] = std::numeric_limits<float>::infinity();
    }
    Buffer result;
    if (!EncodeMotion(action, agent, sequence, result))
        return false;
    if (final) {
        action.id = 15;
        action.state = 0x50;
        action.inputSize = 1;
        action.input[0] = 0x38;
        Buffer finish;
        if (!EncodeMotion(action, agent, sequence, finish) || !result.Append(finish.View()))
            return false;
    }
    out = std::move(result);
    return true;
}
bool SkillActionReply(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                      uint32_t target, uint16_t durationMs, const std::array<float, 3>& position, Buffer& out) {
    return SkillStageReply(agent, sequence, identity, skill, slot, target, 0, true, true, durationMs, position, out);
}
bool MissileCreate(uint16_t agent, uint32_t sequence, uint32_t identity, uint16_t owner, uint32_t skill,
                   const std::array<float, 3>& start, uint32_t token, Buffer& out) {
    return beta::CreateMissile(14812, agent, sequence, identity, owner, skill, start, token, out);
}
bool MissileMove(uint16_t agent, uint32_t sequence, uint32_t identity, uint8_t mode, uint16_t target,
                 const std::array<float, 3>& destination, uint32_t token, float speed, Buffer& out) {
    return beta::MoveMissile(14812, agent, sequence, identity, mode, target, destination, token, speed, out);
}
bool DestroyAgent(uint16_t agent, uint32_t sequence, Buffer& out) {
    return beta::DestroyMissile(14812, agent, sequence, out);
}
bool SkillRecharge(uint32_t character, uint32_t skill, uint32_t remainingMs, uint32_t totalMs, Buffer& out) {
    if (!character || !skill || remainingMs > totalMs)
        return false;
    Writer w;
    w.Word(276);
    w.Var(character);
    w.Var(skill);
    w.Var(remainingMs);
    w.Var(totalMs);
    return w.Finish(out);
}
bool ClearSkillRecharge(uint32_t character, uint32_t skill, Buffer& out) {
    if (!character || !skill)
        return false;
    Writer w;
    w.Word(267);
    w.Var(character);
    w.Var(skill);
    return w.Finish(out);
}
} // namespace july14812
} // namespace glide
