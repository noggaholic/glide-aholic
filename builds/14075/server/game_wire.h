#pragma once
#include "../../14505/server/game_wire.h"

namespace glide {
namespace april14075 {
class GameWire final : public june14505::GameWire {
  public:
    explicit GameWire(const std::string& login) : june14505::GameWire(login, 14075) {}
    DecodeStatus DecodeGame(ByteView input, GameMessage& out) const override;
    bool AllowsDuringLoad(GameConfirmation expected, const GameMessage& received) const override;
    bool AllowsInWorld(const GameMessage& received) const override;
};
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error);
} // namespace april14075
} // namespace glide
