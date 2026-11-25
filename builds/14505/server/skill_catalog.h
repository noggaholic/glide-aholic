#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glide {
namespace june14505 {
struct SkillAnimationPhase {
    uint8_t index = 0, type = 0;
    uint32_t flags = 0, duration = 0;
    float speed = 1;
    std::array<float, 10> movement{};
};
struct SkillCandidate {
    uint32_t loadout = 0, skill = 0, predecessor = 0, recharge = 0;
    uint16_t chainCount = 0, loopMin = 0, loopMax = 0;
    uint8_t weaponSlot = 0, slot = 0;
    bool underwater = false;
    bool hasRange = false;
    float rangeMin = 0, rangeMax = 0;
    std::string professionState, skillFlags, abilityFlags;
    std::vector<SkillAnimationPhase> phases;
};
class SkillCatalog {
  public:
    explicit SkillCatalog(uint32_t build = 14505) : build_(build) {}
    uint32_t Build() const {
        return build_;
    }
    bool Load(const std::wstring& path, std::string& error);
    bool Parse(const std::string& text, std::string& error);
    // Evaluates the exported base palette in authored order. The caller owns
    // chain progress/expiry and supplies the current profession state.
    // Returned pointer remains valid until a successful Load/Parse or destruction.
    const SkillCandidate* Select(uint32_t loadout, uint8_t weaponSlot, uint8_t slot, const std::string& professionState,
                                 uint32_t previousSkill, uint16_t chainCount) const;
    const std::vector<SkillCandidate>& Candidates() const {
        return candidates_;
    }

  private:
    uint32_t build_;
    std::vector<SkillCandidate> candidates_;
};
// Exploration policy: authored minimum loop-step count, or one traversal when
// omitted/zero. This is not evidence of the historical server's scheduling.
bool BuildSkillPlan(const SkillCandidate& skill, std::vector<SkillAnimationPhase>& out, std::string& error);
} // namespace june14505
} // namespace glide
