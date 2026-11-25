#pragma once
#include "../../14505/server/motion_state.h"

namespace glide {
namespace april14075 {
using CharacterAction = june14505::CharacterAction;
// Original April Agent S11 has the same field schema and 80-byte input bound.
// Skill-order payload interpretation remains a separate native verification.
bool EncodeCharacterAction(const CharacterAction& action, uint16_t agent, uint32_t sequence, Buffer& out);
using SkillStage = june14505::SkillStage;
bool EncodeSkillStage(const SkillStage& stage, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                      Buffer& out);
PositionResult MotionPosition(ByteView input, CatalogPosition& out);
FacingResult MotionFacing(ByteView input, Facing& out);
RelayResult RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out);
bool SyncPosition(const CatalogPosition& position, uint16_t agent, uint32_t sequence, Buffer& out);
} // namespace april14075
} // namespace glide
