#pragma once
#include "glide/profile/game_bootstrap.h"

namespace glide {
enum class GameAction {
    Close,
    Control,
    Heartbeat,
    Motion,
    Position,
    Chat,
    Waypoint,
    Hello,
    TransferHello,
    MapContextReady,
    MapLoadAccepted,
    MapLoaderReady,
    WorldBuilt,
    HintCompleted,
    Inventory
};
struct GameMessage {
    uint16_t wireId = 0;
    GameAction action = GameAction::Control;
    size_t consumed = 0;
    uint8_t hint = 0;
};
class WireMap {
  public:
    virtual ~WireMap() {}
    // Unknown length and malformed payloads must close only their connection.
    virtual DecodeStatus DecodeGame(ByteView input, GameMessage& out) const = 0;
    virtual bool BarrierMatches(GameConfirmation expected, const GameMessage& received) const = 0;
    virtual bool AllowsDuringLoad(GameConfirmation, const GameMessage&) const {
        return true;
    }
    // Selected validated controls can be delivered after world entry, in wire order.
    virtual bool DefersDuringLoad(GameConfirmation, const GameMessage&) const {
        return false;
    }
    virtual bool AllowsInWorld(const GameMessage&) const {
        return true;
    }
};
} // namespace glide
