#pragma once
#include "glide/buffer.h"
#include <array>
namespace glide {
namespace july14812 {
struct MotionMessage {
    uint16_t id = 0, agent = 0, scheduleDelay = 0;
    uint32_t sequence = 0, control = 0, token = 0, tick = 0;
    uint8_t state = 0, mode = 0, inputSize = 0, input[80] = {};
    float position[4] = {}, vector[4] = {}, secondary[3] = {}, amount = 0;
    uint16_t words[3] = {};
    uint8_t hasOptional = 0;
    float optionalValue = 0, optionalVector[4] = {}, optionalTail = 0;
    uint16_t optionalWord = 0;
};
struct SkillStage {
    uint8_t index = 0;
    uint16_t durationMs = 0;
    uint8_t type = 0;
    std::array<float, 10> movement{};
};
struct SkillMovement {
    std::array<float, 3> direction{}, facing{};
    float speed = 0;
};
bool BuildSkillMovement(const SkillStage&, const std::array<float, 3>& facing, SkillMovement&, bool underwater = false);
struct SkillPlan {
    uint32_t skill = 0;
    uint8_t slot = 0, count = 0;
    std::array<SkillStage, 32> stages{};
    uint32_t rechargeMs = 0;
    uint8_t state = 0x53;
    bool hasRange = false, underwater = false, instant = false;
    float rangeMin = 0, rangeMax = 0;
};
DecodeStatus DecodeMotion(ByteView, MotionMessage&, size_t& consumed);
bool EncodeMotion(const MotionMessage&, uint16_t agent, uint32_t sequence, Buffer&);
bool RelayMotion(ByteView exactRequest, uint16_t agent, uint32_t sequence, Buffer&);
bool ValidSkillPlan(const SkillPlan&, unsigned slot);
bool SkillStageReply(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                     uint32_t target, uint8_t stage, bool initial, bool final, uint16_t durationMs,
                     const std::array<float, 3>& position, Buffer&, uint8_t state = 0x53,
                     const SkillMovement* movement = nullptr, bool underwater = false);
bool SkillActionReply(uint16_t agent, uint32_t sequence, uint32_t identity, uint32_t skill, uint8_t slot,
                      uint32_t target, uint16_t durationMs, const std::array<float, 3>& position, Buffer&);
// Missile positions are native world units, distinct from Agent physics reports.
bool MissileCreate(uint16_t agent, uint32_t sequence, uint32_t identity, uint16_t owner, uint32_t skill,
                   const std::array<float, 3>& start, uint32_t token, Buffer&);
bool MissileMove(uint16_t agent, uint32_t sequence, uint32_t identity, uint8_t mode, uint16_t target,
                 const std::array<float, 3>& destination, uint32_t token, float speed, Buffer&);
bool DestroyAgent(uint16_t agent, uint32_t sequence, Buffer&);
bool SkillRecharge(uint32_t character, uint32_t skill, uint32_t remainingMs, uint32_t totalMs, Buffer&);
bool ClearSkillRecharge(uint32_t character, uint32_t skill, Buffer&);
} // namespace july14812
} // namespace glide
