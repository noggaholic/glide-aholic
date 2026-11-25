#include "game_wire.h"
#include "control_request.h"
#include "motion.h"
#include "chat.h"
#include "inventory_state.h"
namespace glide {
namespace hot49284 {
DecodeStatus GameWire::DecodeGame(ByteView input, GameMessage& out) const {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    GameMessage message;
    message.wireId = id;
    if (id == 109) {
        uint8_t slot = 0, legend = 0;
        if (!reader.U8(slot) || !reader.U8(legend))
            return DecodeStatus::Incomplete;
        message.action = GameAction::Inventory;
        message.consumed = reader.Position();
        out = message;
        return DecodeStatus::Complete;
    }
    if (id == 56 || id == 94 || id == 98) {
        InventoryRequest request;
        const auto status = DecodeInventoryRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        message.action = GameAction::Inventory;
        message.consumed = request.consumed;
        out = message;
        return status;
    }
    if (id == 154 || id == 155) {
        ChatMessage chat;
        const auto status = DecodeChat(input, chat, message.consumed);
        if (status != DecodeStatus::Complete)
            return status;
        message.action = GameAction::Chat;
        out = message;
        return DecodeStatus::Complete;
    }

    if (id >= 3 && id <= 20) {
        MotionMessage motion{};
        const auto status = DecodeMotion(input, motion, message.consumed);
        if (status != DecodeStatus::Complete)
            return status;
        message.action = id == 16 ? GameAction::Position : GameAction::Motion;
        out = message;
        return DecodeStatus::Complete;
    }

    if (IsSharedControl(id)) {
        const auto status = DecodeControl(input, id, message.consumed);
        if (status != DecodeStatus::Complete)
            return status;
        switch (id) {
        case 1:
            message.action = GameAction::Close;
            break;
        case 2:
            message.action = GameAction::Heartbeat;
            break;
        case 256:
            message.action = GameAction::MapContextReady;
            break;
        case 257:
            message.action = GameAction::Hello;
            break;
        default:
            break;
        }
    } else {
        switch (id) {
        case 247:
            message.action = GameAction::MapLoadAccepted;
            break;
        case 255:
            message.action = GameAction::MapLoaderReady;
            break;
        case 250:
            message.action = GameAction::WorldBuilt;
            break;
        case 253:
            return DecodeStatus::Malformed; // Native streaming failure, never a ready confirmation.
        // C134 is the skillbar's selected-object notification (one VarU32).
        // Actual skill requests still carry and validate their own target.
        case 134:
        case 135:
        case 142: {
            uint32_t object;
            const auto status = reader.VarU32(object);
            if (status != DecodeStatus::Complete)
                return status;
            break;
        }
        case 151: {
            uint16_t context;
            if (!reader.U16(context))
                return DecodeStatus::Incomplete;
            break;
        }
        default:
            return DecodeStatus::Unknown;
        }
        message.consumed = reader.Position();
    }
    out = message;
    return DecodeStatus::Complete;
}
bool GameWire::BarrierMatches(GameConfirmation expected, const GameMessage& message) const {
    switch (expected) {
    case GameConfirmation::Hello:
        return message.wireId == 257 && message.action == GameAction::Hello;
    case GameConfirmation::MapContextReady:
        return message.wireId == 256 && message.action == GameAction::MapContextReady;
    case GameConfirmation::MapLoadAccepted:
        return message.wireId == 247 && message.action == GameAction::MapLoadAccepted;
    case GameConfirmation::MapLoaderReady:
        return message.wireId == 255 && message.action == GameAction::MapLoaderReady;
    case GameConfirmation::WorldBuilt:
        return message.wireId == 250 && message.action == GameAction::WorldBuilt;
    default:
        return false;
    }
}
bool GameWire::AllowsDuringLoad(GameConfirmation expected, const GameMessage& message) const {
    if (message.action == GameAction::Close)
        return true;
    if (BarrierMatches(expected, message))
        return true;
    if (message.wireId >= 163 && message.wireId <= 166)
        return true;
    if (message.wireId == 2)
        return expected != GameConfirmation::Hello && expected != GameConfirmation::TransferHello;
    // The native client can report its delay after the actor arrives, before
    // C250. This four-byte C15 is informational and never opens the gate.
    return (message.wireId == 15 || message.wireId == 151) && expected == GameConfirmation::WorldBuilt;
}
bool GameWire::AllowsInWorld(const GameMessage& message) const {
    switch (message.wireId) {
    case 257:
    case 256:
    case 247:
    case 255:
    case 250:
    case 253:
        return false;
    default:
        return true;
    }
}
} // namespace hot49284
} // namespace glide
