#pragma once
#include "glide/profile/wire_map.h"
namespace glide {
namespace hot49284 {
// Connection authorization is performed by the bound Game initializer.
class GameWire : public WireMap {
  public:
    DecodeStatus DecodeGame(ByteView input, GameMessage& out) const override;
    bool BarrierMatches(GameConfirmation expected, const GameMessage& received) const override;
    bool AllowsDuringLoad(GameConfirmation, const GameMessage&) const override;
    bool AllowsInWorld(const GameMessage&) const override;
};
} // namespace hot49284
} // namespace glide
