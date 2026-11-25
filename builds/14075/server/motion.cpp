#include "motion.h"

namespace glide {
namespace april14075 {
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out) {
    return june14505::DecodeMotionForBuild(input, 14075, out);
}
} // namespace april14075
} // namespace glide
