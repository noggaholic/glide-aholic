#include "game_wire.h"
#include "inventory_state.h"
#include "game_request.h"
#include "world_protocol.h"
#include "glide/files.h"
#include <Windows.h>

namespace glide {
namespace june14505 {
bool CreateGameWireForBuild(unsigned build, const std::string& login, std::unique_ptr<WireMap>& out,
                            std::string& error) {
    std::wstring decoded;
    if (!WorldProtocolBuild(build) || login.empty() || login.find('\0') != std::string::npos ||
        !Utf8ToWide(login, decoded) || decoded.size() > 63) {
        error = "Invalid account for June world connection";
        return false;
    }
    out.reset(new GameWire(login, build));
    error.clear();
    return true;
}
DecodeStatus GameWire::DecodeGame(ByteView input, GameMessage& out) const {
    if (build_ == 14422 || build_ == 14505) {
        InventoryRequest inventory;
        const auto decoded = DecodeInventoryRequest(build_, input, inventory);
        if (decoded != DecodeStatus::Unknown) {
            if (decoded != DecodeStatus::Complete)
                return decoded;
            Reader reader(input);
            uint16_t id = 0;
            if (!reader.U16(id))
                return DecodeStatus::Incomplete;
            out = GameMessage{id, GameAction::Inventory, inventory.consumed};
            return DecodeStatus::Complete;
        }
    }
    GameRequest request;
    const auto status = DecodeGameRequestForBuild(input, build_, request);
    if (status != DecodeStatus::Complete)
        return status;
    GameAction action = GameAction::Control;
    switch (request.kind) {
    case GameRequestKind::Motion:
        action = request.motion.canonicalId == 0        ? GameAction::Control
                 : request.motion.canonicalId == 0x000d ? GameAction::Position
                                                        : GameAction::Motion;
        break;
    case GameRequestKind::ChatTarget:
    case GameRequestKind::ChatChannel:
    case GameRequestKind::ServerCommand:
        action = GameAction::Chat;
        break;
    case GameRequestKind::Waypoint:
        action = GameAction::Waypoint;
        break;
    default:
        break;
    }
    const auto& message = request.mission;
    if (request.kind == GameRequestKind::Mission)
        switch (message.action) {
        case MissionAction::Identity: {
            std::wstring expected, received;
            if (login_.empty() || !Utf8ToWide(login_, expected) || !Utf8ToWide(message.login, received) ||
                CompareStringOrdinal(expected.c_str(), -1, received.c_str(), -1, TRUE) != CSTR_EQUAL)
                return DecodeStatus::Malformed;
            action = GameAction::Hello;
            break;
        }
        case MissionAction::Close:
            action = GameAction::Close;
            break;
        case MissionAction::MapAccepted:
            action = GameAction::MapContextReady;
            break;
        case MissionAction::SpawnAccepted:
            action = GameAction::MapLoadAccepted;
            break;
        case MissionAction::MapLoaded:
            action = GameAction::MapLoaderReady;
            break;
        case MissionAction::WorldBuilt:
            action = GameAction::WorldBuilt;
            break;
        case MissionAction::Heartbeat:
            action = GameAction::Heartbeat;
            break;
        default:
            break;
        }
    out = GameMessage{request.id, action, request.consumed};
    return DecodeStatus::Complete;
}
bool GameWire::BarrierMatches(GameConfirmation expected, const GameMessage& received) const {
    switch (expected) {
    case GameConfirmation::Hello:
        return received.action == GameAction::Hello;
    case GameConfirmation::MapContextReady:
        return received.action == GameAction::MapContextReady;
    case GameConfirmation::MapLoadAccepted:
        return received.action == GameAction::MapLoadAccepted;
    case GameConfirmation::MapLoaderReady:
        return received.action == GameAction::MapLoaderReady;
    case GameConfirmation::WorldBuilt:
        return received.action == GameAction::WorldBuilt;
    default:
        return false;
    }
}
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) {
    return CreateGameWireForBuild(14505, login, out, error);
}
} // namespace june14505
} // namespace glide
