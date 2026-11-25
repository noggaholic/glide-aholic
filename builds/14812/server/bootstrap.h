#pragma once
#include "world_actor.h"
#include "glide/profile/game_bootstrap.h"
#include <memory>
namespace glide {
namespace july14812 {
bool TimeOfDay(uint32_t ticks, float rate, bool paused, Buffer&);
// Eight original July maps use their own checked resources and native bounds.
bool CreateBootstrap(CharacterCatalog, std::unique_ptr<GameBootstrap>&, std::string& error);
bool LoadBootstrap(const std::wstring& dataRoot, std::unique_ptr<GameBootstrap>&, std::string& error);
} // namespace july14812
} // namespace glide
