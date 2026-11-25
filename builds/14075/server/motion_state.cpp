#include "motion_state.h"

namespace glide {
namespace april14075 {
bool EncodeCharacterAction(const CharacterAction& action, uint16_t agent, uint32_t sequence, Buffer& out) {
    return june14505::EncodeCharacterAction(action, agent, sequence, out);
}
bool EncodeSkillStage(const SkillStage& stage, const CharacterAction& action, uint16_t agent, uint32_t sequence,
                      Buffer& out) {
    return june14505::EncodeSkillStageForBuild(14075, stage, action, agent, sequence, out);
}
PositionResult MotionPosition(ByteView input, CatalogPosition& out) {
    return june14505::MotionPositionForBuild(input, 14075, out);
}
FacingResult MotionFacing(ByteView input, Facing& out) {
    return june14505::MotionFacingForBuild(input, 14075, out);
}
RelayResult RelayMotion(ByteView input, uint16_t agent, uint32_t sequence, Buffer& out) {
    return june14505::RelayMotionForBuild(input, 14075, agent, sequence, out);
}
bool SyncPosition(const CatalogPosition& position, uint16_t agent, uint32_t sequence, Buffer& out) {
    return june14505::SyncPositionForBuild(14075, position, agent, sequence, out);
}
} // namespace april14075
} // namespace glide
