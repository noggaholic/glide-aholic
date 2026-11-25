#pragma once
#include "motion_state.h"
#include "../../14505/server/skill_execution.h"

namespace glide {
namespace april14075 {
using SkillPhase = june14505::SkillPhase;
using SkillProgress = june14505::SkillProgress;
// Preserve the shared bounded scheduler while using April's own order and
// completion identifiers. Actual plan selection requires the own ready catalog.
class SkillExecution final : public june14505::SkillExecution {
  public:
    SkillExecution() : june14505::SkillExecution(14075) {}
};
} // namespace april14075
} // namespace glide
