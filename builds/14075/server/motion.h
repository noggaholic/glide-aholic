#pragma once
#include "../../14505/server/motion.h"

namespace glide {
namespace april14075 {
using MotionMessage = june14505::MotionMessage;
DecodeStatus DecodeMotion(ByteView input, MotionMessage& out);
} // namespace april14075
} // namespace glide
