#pragma once
#include "glide/profile/wire_map.h"
#include <memory>

namespace glide {
namespace june14505 {
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error);
bool CreateGameWireForBuild(unsigned build, const std::string& login, std::unique_ptr<WireMap>& out,
                            std::string& error);
// Bound to the already authorized account. Does not authorize a connection itself.
class GameWire : public WireMap {
  public:
    explicit GameWire(const std::string& login, unsigned build = 14505) : login_(login), build_(build) {}
    DecodeStatus DecodeGame(ByteView input, GameMessage& out) const override;
    bool BarrierMatches(GameConfirmation expected, const GameMessage& received) const override;
    bool AllowsDuringLoad(GameConfirmation, const GameMessage& received) const override {
        return received.action != GameAction::Inventory;
    }

  private:
    std::string login_;
    unsigned build_;
};
} // namespace june14505
} // namespace glide
