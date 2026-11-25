#include "motion_state.h"
#include "motion.h"
#include "world_protocol.h"
#include <cmath>
#include <limits>
#include <cstring>

namespace glide {
namespace june14505 {
namespace {
// Shared by client movement relay and authoritative skill movement.
bool EncodeMovement(unsigned build, const MotionMessage& message, uint16_t agent, uint32_t sequence, Buffer& out) {
    Buffer result;
    if (!WorldProtocolBuild(build) || !agent || !sequence || !message.input.Valid() || message.input.size > 80 ||
        !result.U16(build == 14075 ? 25 : 27) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(message.state) || !result.VarU32(message.flags) ||
        !result.U8(static_cast<uint8_t>(message.input.size)) || !result.Append(message.input))
        return false;
    for (float v : message.position)
        if (!std::isfinite(v) || !result.F32(v))
            return false;
    for (float v : message.vectorA)
        if (!std::isfinite(v) || !result.F32(v))
            return false;
    for (float v : message.vectorB)
        if (!std::isfinite(v) || !result.F32(v))
            return false;
    if (!result.U16(message.wordA) || !result.U16(message.wordB) || !result.U8(message.hasOptional ? 1 : 0))
        return false;
    if (message.hasOptional) {
        if (!result.U32(message.optionalValue) || !result.U16(message.optionalWord))
            return false;
        for (float v : message.optionalVector)
            if ((!std::isfinite(v) && v != std::numeric_limits<float>::infinity()) || !result.F32(v))
                return false;
    }
    out = std::move(result);
    return true;
}
} // namespace
bool EncodeSkillEndForBuild(unsigned build, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                            Buffer& out) {
    Buffer result;
    if (!WorldProtocolBuild(build) || !agent || !sequence || !action.identity || (action.identity & 0x80000000u) ||
        !result.U16(build == 14075 ? 39 : 48) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(static_cast<uint8_t>(action.state & 0xfcu)) || !result.VarU32(action.identity) || !result.U8(1) ||
        !result.U8(build == 14075 ? 0x37 : 0x39))
        return false;
    out = std::move(result);
    return true;
}
bool EncodeSkillStage(const SkillStage& stage, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                      Buffer& out) {
    return EncodeSkillStageForBuild(14505, stage, action, agent, sequence, out);
}
bool EncodeSkillStageForBuild(unsigned build, const SkillStage& stage, const CharacterAction& action, uint16_t agent,
                              uint32_t sequence, Buffer& out) {
    if (!WorldProtocolBuild(build) || !stage.skill || stage.slot >= 17 || !action.identity ||
        (action.identity & 0x80000000u))
        return false;
    Buffer input;
    if (!input.U8(build == 14075 ? 0x22 : 0x23) || !input.U8(stage.initial ? 0 : 4))
        return false;
    if (stage.initial) {
        if (!input.U32(stage.skill) || !input.U8(stage.slot) || !input.U8(stage.index) || !input.U32(stage.target) ||
            !input.U8(stage.underwater ? 3 : 1))
            return false;
    } else if (!input.U8(stage.index) || !input.U8(3))
        return false;
    CharacterAction next = action;
    next.input = input.View();
    Buffer result;
    if (action.moving) {
        if (!std::isfinite(action.movementSpeed) || action.movementSpeed <= 0 || !action.words[2])
            return false;
        MotionMessage move;
        move.state = action.state;
        move.flags = action.identity;
        move.input = input.View();
        move.position = action.pose;
        move.vectorA = action.vector;
        move.vectorB = action.direction;
        move.wordA = action.words[0];
        move.wordB = action.words[1];
        move.hasOptional = true;
        std::memcpy(&move.optionalValue, &action.movementSpeed, sizeof(float));
        move.optionalWord = action.words[2];
        for (unsigned i = 0; i < 3; ++i)
            move.optionalVector[i] = std::numeric_limits<float>::infinity();
        if (!EncodeMovement(build, move, agent, sequence, result))
            return false;
    } else if (!EncodeCharacterAction(next, agent, sequence, result))
        return false;
    // Finish the same authoritative order, on the same lane, after its last phase.
    // Low control-acquire/release bits belong to the action, not this signal.
    if (stage.final) {
        Buffer finish;
        if (!EncodeSkillEndForBuild(build, action, agent, sequence, finish) || !result.Append(finish.View()))
            return false;
    }
    out = std::move(result);
    return true;
}
bool EncodeCharacterAction(const CharacterAction& action, uint16_t agent, uint32_t sequence, Buffer& out) {
    if (!agent || !sequence || !action.input.Valid() || action.input.size > 80)
        return false;
    for (float value : action.pose)
        if (!std::isfinite(value))
            return false;
    for (float value : action.vector)
        if (!std::isfinite(value))
            return false;
    Buffer result;
    if (!result.U16(11) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(action.state) ||
        !result.VarU32(action.identity) || !result.U8(static_cast<uint8_t>(action.input.size)) ||
        !result.Append(action.input))
        return false;
    for (float value : action.pose)
        if (!result.F32(value))
            return false;
    for (float value : action.vector)
        if (!result.F32(value))
            return false;
    for (uint16_t value : action.words)
        if (!result.U16(value))
            return false;
    out = std::move(result);
    return true;
}
PositionResult MotionPositionForBuild(ByteView input, unsigned build, CatalogPosition& out) {
    MotionMessage message;
    const auto status = DecodeMotionForBuild(input, build, message);
    if (status == DecodeStatus::Unknown)
        return PositionResult::Absent;
    if (status != DecodeStatus::Complete || message.consumed != input.size)
        return PositionResult::Invalid;
    if (message.canonicalId == 0 || message.canonicalId == 3 || message.canonicalId == 5 || message.canonicalId == 9 ||
        message.canonicalId == 15)
        return PositionResult::Absent;
    out = CatalogPosition{message.position[0] / 32, message.position[1] / 32,
                          message.position[2] == 0 ? 0 : -message.position[2] / 32};
    return PositionResult::Present;
}
FacingResult MotionFacingForBuild(ByteView input, unsigned build, Facing& out) {
    MotionMessage message;
    const auto status = DecodeMotionForBuild(input, build, message);
    if (status == DecodeStatus::Unknown)
        return FacingResult::Absent;
    if (status != DecodeStatus::Complete || message.consumed != input.size)
        return FacingResult::Invalid;
    if (message.canonicalId != 8 || message.wordA == 0x7fff || message.wordB == 0x7fff)
        return FacingResult::Absent;
    const int yawWord = message.wordB < 0x8000 ? message.wordB : static_cast<int>(message.wordB) - 65536;
    const int pitchWord = message.wordA < 0x8000 ? message.wordA : static_cast<int>(message.wordA) - 65536;
    const double yaw = yawWord * 3.14159265358979323846 / 32766;
    const double pitch = pitchWord * 3.14159265358979323846 / 32766;
    out = Facing{static_cast<float>(std::cos(yaw) * std::cos(pitch)),
                 static_cast<float>(std::sin(yaw) * std::cos(pitch)), static_cast<float>(std::sin(pitch))};
    return FacingResult::Present;
}
RelayResult RelayMotionForBuild(ByteView input, unsigned build, uint16_t agent, uint32_t sequence, Buffer& out) {
    MotionMessage message;
    const auto status = DecodeMotionForBuild(input, build, message);
    if (status == DecodeStatus::Unknown)
        return RelayResult::Ignored;
    if (status != DecodeStatus::Complete || message.consumed != input.size)
        return RelayResult::Invalid;
    // The old server consumes steering without sending a state response. Periodic
    // position reports update presence separately from the detailed motion stream.
    if (message.canonicalId == 0 || message.canonicalId == 9 || message.canonicalId == 13)
        return RelayResult::Ignored;
    if (!agent || !sequence)
        return RelayResult::Invalid;
    if (message.canonicalId == 8)
        return EncodeMovement(build, message, agent, sequence, out) ? RelayResult::Encoded : RelayResult::Invalid;
    if (message.canonicalId == 6) {
        CharacterAction action;
        action.state = message.state;
        action.identity = message.flags;
        action.input = message.input;
        action.pose = message.position;
        action.vector = message.vectorA;
        action.words = {{message.wordA, message.wordB, message.optionalWord}};
        return EncodeCharacterAction(action, agent, sequence, out) ? RelayResult::Encoded : RelayResult::Invalid;
    }
    if (message.canonicalId == 5) {
        Buffer action;
        if (!action.U16(10) || !action.U16(agent) || !action.VarU32(sequence) || !action.U8(message.state) ||
            !action.VarU32(message.flags) || !action.U8(static_cast<uint8_t>(message.input.size)) ||
            !action.Append(message.input) || !action.U16(message.wordA))
            return RelayResult::Invalid;
        out = std::move(action);
        return RelayResult::Encoded;
    }
    const uint16_t id = message.canonicalId == 3    ? 8
                        : message.canonicalId == 4  ? 9
                        : message.canonicalId == 7  ? (build == 14075 ? 0x18 : 0x19)
                        : message.canonicalId == 15 ? (build == 14075 ? 39 : 48)
                                                    : (build == 14075 ? 0x19 : 0x1b);
    Buffer result;
    if (!result.U16(id) || !result.U16(agent) || !result.VarU32(sequence) || !result.U8(message.state))
        return RelayResult::Invalid;
    if (message.canonicalId == 7 || message.canonicalId == 8 || message.canonicalId == 15)
        if (!result.VarU32(message.flags) || !result.U8(static_cast<uint8_t>(message.input.size)) ||
            !result.Append(message.input))
            return RelayResult::Invalid;
    if (message.canonicalId != 3 && message.canonicalId != 15) {
        for (float value : message.position)
            if (!result.F32(value))
                return RelayResult::Invalid;
        for (float value : message.vectorA)
            if (!result.F32(value))
                return RelayResult::Invalid;
    }
    out = std::move(result);
    return RelayResult::Encoded;
}
bool SyncPositionForBuild(unsigned build, const CatalogPosition& position, uint16_t agent, uint32_t sequence,
                          Buffer& out) {
    const float values[] = {position.x * 32, position.y * 32,
                            build == 14075 ? position.z * -32 : (position.z == 0 ? 0 : -position.z * 32), 0};
    if (!WorldProtocolBuild(build) || !agent || !sequence)
        return false;
    for (float value : values)
        if (!std::isfinite(value))
            return false;
    Buffer result;
    if (!result.U16(build == 14075 ? 0x0028 : 0x0031) || !result.U16(agent) || !result.VarU32(sequence) ||
        !result.U8(0x40))
        return false;
    for (float value : values)
        if (!result.F32(value))
            return false;
    out = std::move(result);
    return true;
}
PositionResult MotionPosition(ByteView input, CatalogPosition& out) {
    return MotionPositionForBuild(input, 14505, out);
}
FacingResult MotionFacing(ByteView input, Facing& out) {
    return MotionFacingForBuild(input, 14505, out);
}
RelayResult RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out) {
    return RelayMotionForBuild(input, 14505, agent, sequence, out);
}
bool SyncPosition(const CatalogPosition& position, uint16_t agent, uint32_t sequence, Buffer& out) {
    return SyncPositionForBuild(14505, position, agent, sequence, out);
}
} // namespace june14505
} // namespace glide
