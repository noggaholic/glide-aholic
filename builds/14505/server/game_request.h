#pragma once
#include "mission.h"
#include "motion.h"
#include <vector>

namespace glide {
namespace june14505 {
enum class GameRequestKind {
    Mission,
    Motion,
    ChatTarget,
    ChatChannel,
    Waypoint,
    Hint,
    Debug,
    SkillbarRefresh,
    ServerCommand
};
struct GameRequest {
    GameRequestKind kind = GameRequestKind::Mission;
    uint16_t id = 0;
    uint32_t value = 0;
    uint8_t channel = 0;
    std::string text;
    std::vector<uint32_t> values;
    MissionMessage mission;
    MotionMessage motion;
    size_t consumed = 0;
};
DecodeStatus DecodeGameRequest(ByteView input, GameRequest& out);
DecodeStatus DecodeGameRequestForBuild(ByteView input, unsigned build, GameRequest& out);
} // namespace june14505
} // namespace glide
