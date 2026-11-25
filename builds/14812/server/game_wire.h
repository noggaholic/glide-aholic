#pragma once
#include "glide/profile/wire_map.h"
namespace glide {
namespace july14812 {
// July world/scene controls, all fourteen Agent requests and native chat requests.
class GameWire final : public WireMap {
  public:
    DecodeStatus DecodeGame(ByteView, GameMessage&) const override;
    bool BarrierMatches(GameConfirmation, const GameMessage&) const override;
    bool AllowsDuringLoad(GameConfirmation, const GameMessage&) const override;
    bool DefersDuringLoad(GameConfirmation, const GameMessage&) const override;
    bool AllowsInWorld(const GameMessage&) const override;
};
} // namespace july14812
} // namespace glide
