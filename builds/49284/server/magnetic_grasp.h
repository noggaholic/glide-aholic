#pragma once
#include "magnetic_grasp_rules.h"
#include "glide/profile/target_link.h"
namespace glide {
namespace hot49284 {
struct MagneticGraspRules {
    enum : unsigned {
        Marker = kMagneticMarker,
        ReleaseMs = kMagneticReleaseMs,
        WindowMs = kMagneticWindowMs,
        Range = kMagneticRange,
        Instance = UINT32_MAX - 2
    };
};
template <class Rules> using TargetLink = glide::TargetLink<Rules>;
using MagneticGrasp = TargetLink<MagneticGraspRules>;
} // namespace hot49284
} // namespace glide
