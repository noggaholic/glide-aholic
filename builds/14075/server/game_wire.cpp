#include "game_wire.h"
#include "skill_request.h"
#include "inventory_wire.h"

namespace glide {
namespace april14075 {
DecodeStatus GameWire::DecodeGame(ByteView input, GameMessage& out) const {
    Reader reader(input);
    uint16_t id = 0;
    const bool header = input.Valid() && reader.U16(id);
    if (header && id == 31) {
        InventoryMoveRequest request;
        const auto parsed = DecodeInventoryMoveRequest(input, request);
        if (parsed != DecodeStatus::Complete)
            return parsed;
        out = GameMessage{id, GameAction::Inventory, request.consumed};
        return DecodeStatus::Complete;
    }
    if (header && (id == 55 || id == 56)) {
        InventoryEquipRequest request;
        const auto parsed =
            id == 55 ? DecodeInventoryEquipRequest(input, request) : DecodeInventoryUnequipRequest(input, request);
        if (parsed != DecodeStatus::Complete)
            return parsed;
        out = GameMessage{id, GameAction::Inventory, request.consumed};
        return DecodeStatus::Complete;
    }
    GameMessage next;
    const auto status = june14505::GameWire::DecodeGame(input, next);
    if (status != DecodeStatus::Complete)
        return status;
    if (next.action == GameAction::Motion && next.wireId == 16) {
        SkillRequest request;
        const auto parsed = DecodeSkillRequest(ByteView(input.data, next.consumed), request);
        if (parsed != DecodeStatus::Complete && parsed != DecodeStatus::Unknown)
            return parsed;
    }
    out = std::move(next);
    return DecodeStatus::Complete;
}
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) {
    std::unique_ptr<WireMap> validated;
    if (!june14505::CreateGameWireForBuild(14075, login, validated, error))
        return false;
    out.reset(new GameWire(login));
    return true;
}
bool GameWire::AllowsDuringLoad(GameConfirmation expected, const GameMessage& received) const {
    if (received.action == GameAction::Inventory)
        return false;
    if (received.action == GameAction::Close || received.action == GameAction::Heartbeat)
        return true;
    if (BarrierMatches(expected, received))
        return true;
    if (received.action == GameAction::Control && received.wireId == 0x004f)
        return true;
    return expected == GameConfirmation::WorldBuilt &&
           ((received.action == GameAction::Control && (received.wireId == 0 || received.wireId == 0x008e)) ||
            received.action == GameAction::Motion || received.action == GameAction::Position);
}
bool GameWire::AllowsInWorld(const GameMessage& received) const {
    switch (received.action) {
    case GameAction::Hello:
    case GameAction::TransferHello:
    case GameAction::MapContextReady:
    case GameAction::MapLoadAccepted:
    case GameAction::MapLoaderReady:
    case GameAction::WorldBuilt:
        return false;
    default:
        return true;
    }
}
} // namespace april14075
} // namespace glide
