#pragma once
#include "glide/profile/auth_flow.h"
#include "glide/profile/character_codec.h"
#include <functional>

namespace glide {
namespace china167409 {
std::unique_ptr<glide::AuthFlow> CreateAuthFlow(std::function<uint64_t()> unixSeconds,
                                                std::function<uint32_t()> tickMilliseconds,
                                                std::unique_ptr<CharacterCodec> characters);
}
} // namespace glide
