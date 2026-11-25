#include "game_wire.h"
#include "inventory_state.h"
#include "create.h"
#include "motion.h"
#include "chat.h"
#include "auth_bytes.h"
namespace glide {
namespace july14812 {
DecodeStatus GameWire::DecodeGame(ByteView input, GameMessage& out) const {
    if (!input.Valid())
        return DecodeStatus::Malformed;
    Reader reader(input);
    uint16_t id;
    if (!reader.U16(id))
        return DecodeStatus::Incomplete;
    GameMessage result;
    result.wireId = id;
    if (id == 33 || id == 56 || id == 57) {
        InventoryRequest request;
        const auto status = DecodeInventoryRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        result.action = GameAction::Inventory;
        result.consumed = request.consumed;
        out = result;
        return DecodeStatus::Complete;
    }
    if (id == 83 || id == 84) {
        ChatMessage chat;
        const auto status = DecodeChat(input, chat, result.consumed);
        if (status != DecodeStatus::Complete)
            return status;
        result.action = GameAction::Chat;
        out = result;
        return DecodeStatus::Complete;
    }
    if (id >= 3 && id <= 16) {
        MotionMessage motion;
        const auto status = DecodeMotion(input, motion, result.consumed);
        if (status != DecodeStatus::Complete)
            return status;
        result.action = id == 13 ? GameAction::Position : GameAction::Motion;
        out = result;
        return DecodeStatus::Complete;
    }
    switch (id) {
    case 75: {
        // Native C75 contains one VarU32. Observed during the two-player skill
        // review; accept its framing without granting any gameplay action.
        uint32_t value = 0;
        const auto status = reader.VarU32(value);
        if (status != DecodeStatus::Complete)
            return status;
        result.action = GameAction::Control;
        break;
    }
    case 65: {
        // Native C65: field type21, uint16 byte count, at most8064 bytes.
        // The client emits this control after changing its inventory layout.
        // As with the corresponding China control, no game state is changed.
        uint16_t length = 0;
        if (!reader.U16(length))
            return DecodeStatus::Incomplete;
        if (length > 8064)
            return DecodeStatus::Malformed;
        if (reader.Remaining() < length)
            return DecodeStatus::Incomplete;
        result.action = GameAction::Control;
        result.consumed = reader.Position() + length;
        out = result;
        return DecodeStatus::Complete;
    }
    case 86: {
        // Original July server-command schema: UTF16[200], then VarU32.
        // Validate framing and keep unknown commands private; no chat routing.
        std::string ignored;
        uint32_t trailing = 0;
        auto status = detail::Text(reader, 201, ignored);
        if (status != DecodeStatus::Complete)
            return status;
        status = reader.VarU32(trailing);
        if (status != DecodeStatus::Complete)
            return status;
        break;
    }
    case 28: {
        uint32_t hint;
        const auto status = reader.VarU32(hint);
        if (status != DecodeStatus::Complete)
            return status;
        if (hint > 255)
            return DecodeStatus::Malformed;
        result.action = GameAction::HintCompleted;
        result.hint = static_cast<uint8_t>(hint);
        break;
    }
    case 143:
        result.action = GameAction::MapLoadAccepted;
        break;
    case 146:
        result.action = GameAction::WorldBuilt;
        break;
    case 151:
        result.action = GameAction::MapLoaderReady;
        break;
    case 1:
    case 2:
    case 89:
    case 120:
    case 154:
    case 155: {
        CreateMessage control;
        const auto status = DecodeSceneControl(input, control);
        if (status != DecodeStatus::Complete)
            return status;
        result.consumed = control.consumed;
        if (id == 1)
            result.action = GameAction::Close;
        else if (id == 2)
            result.action = GameAction::Heartbeat;
        else if (id == 154)
            result.action = GameAction::MapContextReady;
        else if (id == 155)
            result.action = GameAction::Hello;
        out = result;
        return DecodeStatus::Complete;
    }
    default:
        return DecodeStatus::Unknown;
    }
    result.consumed = reader.Position();
    out = result;
    return DecodeStatus::Complete;
}
bool GameWire::BarrierMatches(GameConfirmation expected, const GameMessage& m) const {
    switch (expected) {
    case GameConfirmation::Hello:
        return m.wireId == 155 && m.action == GameAction::Hello;
    case GameConfirmation::MapContextReady:
        return m.wireId == 154 && m.action == GameAction::MapContextReady;
    case GameConfirmation::MapLoadAccepted:
        return m.wireId == 143 && m.action == GameAction::MapLoadAccepted;
    case GameConfirmation::MapLoaderReady:
        return m.wireId == 151 && m.action == GameAction::MapLoaderReady;
    case GameConfirmation::WorldBuilt:
        return m.wireId == 146 && m.action == GameAction::WorldBuilt;
    default:
        return false;
    }
}
bool GameWire::AllowsDuringLoad(GameConfirmation expected, const GameMessage& m) const {
    if (m.action == GameAction::Close || BarrierMatches(expected, m) || m.wireId == 120)
        return true;
    if (m.wireId == 2)
        return expected != GameConfirmation::Hello && expected != GameConfirmation::TransferHello;
    if (m.wireId == 12 && m.action == GameAction::Motion)
        return expected == GameConfirmation::WorldBuilt;
    return m.wireId == 28 && expected == GameConfirmation::WorldBuilt;
}
bool GameWire::DefersDuringLoad(GameConfirmation expected, const GameMessage& m) const {
    // The native scheduler can report its delay before C146 on a cached map.
    // Keep the exact bytes for the normal world action handler after C146.
    return expected == GameConfirmation::WorldBuilt && m.wireId == 12 && m.action == GameAction::Motion;
}
bool GameWire::AllowsInWorld(const GameMessage& m) const {
    if (m.wireId == 65 || m.wireId == 75)
        return m.action == GameAction::Control;
    if (m.action == GameAction::Inventory)
        return m.wireId == 33 || m.wireId == 56 || m.wireId == 57;
    return (m.wireId >= 3 && m.wireId <= 16) || m.wireId == 83 || m.wireId == 84 || m.wireId == 86 || m.wireId == 1 ||
           m.wireId == 2 || m.wireId == 28 || m.wireId == 89 || m.wireId == 120;
}
} // namespace july14812
} // namespace glide
