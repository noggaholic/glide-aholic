#pragma once
#include "glide/profile/game_messages.h"
#include <array>

namespace glide {
namespace june14505 {
PositionResult MotionPosition(ByteView input, CatalogPosition& out);
FacingResult MotionFacing(ByteView input, Facing& out);
RelayResult RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out);
bool SyncPosition(const CatalogPosition& position, uint16_t agent, uint32_t sequence, Buffer& out);
PositionResult MotionPositionForBuild(ByteView input, unsigned build, CatalogPosition& out);
FacingResult MotionFacingForBuild(ByteView input, unsigned build, Facing& out);
RelayResult RelayMotionForBuild(ByteView input, unsigned build, uint16_t agent, uint32_t sequence, Buffer& out);
bool SyncPositionForBuild(unsigned build, const CatalogPosition& position, uint16_t agent, uint32_t sequence,
                          Buffer& out);
// Native S11 fields. Pose/vector use client units, without catalog conversion.
// Scheduling sequence and order identity are separate protocol values.
struct CharacterAction {
    uint8_t state = 0;
    uint32_t identity = 0;
    ByteView input;
    std::array<float, 4> pose{};
    std::array<float, 3> vector{};
    std::array<uint16_t, 3> words{};
    bool moving = false;
    std::array<float, 3> direction{};
    float movementSpeed = 0;
};
bool EncodeCharacterAction(const CharacterAction& action, uint16_t agent, uint32_t sequence, Buffer& out);
struct SkillStage {
    uint32_t skill = 0, target = 0;
    uint8_t slot = 0, index = 0;
    bool initial = true, final = false, underwater = false;
};
// The caller supplies scheduling, lane/control state, pose and duration words.
// Initial stages replace input with a native skill order; later stages update it.
bool EncodeSkillStage(const SkillStage& stage, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                      Buffer& out);
bool EncodeSkillStageForBuild(unsigned build, const SkillStage& stage, const CharacterAction& action, uint16_t agent,
                              uint32_t sequence, Buffer& out);
bool EncodeSkillEndForBuild(unsigned build, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                            Buffer& out);
} // namespace june14505
} // namespace glide
