#pragma once

namespace glide {
namespace hot49284 {
static const unsigned kBubbleSkill = 5652, kBubbleFollowup = 5653, kBubbleMarker = 5654, kBubbleLaunchMs = 500,
                      kBubbleRange = 1200;
struct BubbleRule {
    unsigned skill, followup, marker, slot, attunement, launchMs, range;
};
static const BubbleRule kBubbleRules[] = {{5652, 5653, 5654, 7, 3, 500, 1200}, {5605, 5611, 5612, 6, 2, 500, 1200}};
inline const BubbleRule* BubbleForSkill(unsigned skill) {
    for (const auto& r : kBubbleRules)
        if (r.skill == skill)
            return &r;
    return nullptr;
}
} // namespace hot49284
} // namespace glide
