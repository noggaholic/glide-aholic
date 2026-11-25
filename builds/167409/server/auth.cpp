#include "auth.h"
#include "auth_request.h"
#include "glide/files.h"

namespace glide {
const AuthCodec& Build204489Auth();
namespace {
bool Ack(uint32_t request, Buffer& out, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
    return out.U16(4) && out.VarU32(request) && out.VarU32(0) && out.VarU32(a) && out.VarU32(b) && out.VarU32(c);
}
class ChinaAuth final : public AuthCodec {
  public:
    bool Summary(const Character& character, Buffer& out) const override {
        return Build204489Auth().Summary(character, out);
    }
    DecodeStatus Decode(ByteView input, bool authenticated, AuthMessage& out) const override {
        china167409::AuthRequest request;
        const auto status = china167409::DecodeAuthRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        AuthMessage result;
        result.id = request.id;
        result.consumed = request.consumed;
        result.action = AuthAction::Control;
        Reader reader(request.payload);
        switch (request.id) {
        case 0x000a:
            if (authenticated)
                return DecodeStatus::Malformed;
            if (reader.VarU32(result.request) != DecodeStatus::Complete || !reader.Bytes(32, result.identity))
                return DecodeStatus::Malformed;
            result.action = AuthAction::Login;
            break;
        case 0x0003:
            result.action = AuthAction::SessionStatus;
            break;
        case 0x000c:
        case 0x000e:
            if (reader.VarU32(result.request) != DecodeStatus::Complete)
                return DecodeStatus::Malformed;
            result.action = AuthAction::Reply;
            break;
        case 0x0014:
        case 0x0016: {
            if (!authenticated || reader.VarU32(result.request) != DecodeStatus::Complete)
                return DecodeStatus::Malformed;
            std::wstring name;
            uint16_t word;
            while (reader.U16(word) && word)
                name.push_back(static_cast<wchar_t>(word));
            if (!WideToUtf8(name, result.characterName))
                return DecodeStatus::Malformed;
            result.action = request.id == 0x0014 ? AuthAction::DeleteCharacter : AuthAction::SelectCharacter;
            break;
        }
        case 0x0019:
            if (!authenticated || reader.VarU32(result.request) != DecodeStatus::Complete)
                return DecodeStatus::Malformed;
            result.action = AuthAction::CharacterList;
            break;
        case 0x001d: {
            uint32_t ignored;
            if (!authenticated || reader.VarU32(result.request) != DecodeStatus::Complete ||
                reader.VarU32(result.route) != DecodeStatus::Complete ||
                reader.VarU32(ignored) != DecodeStatus::Complete ||
                reader.VarU32(result.map) != DecodeStatus::Complete ||
                reader.VarU32(ignored) != DecodeStatus::Complete ||
                reader.VarU32(result.context) != DecodeStatus::Complete)
                return DecodeStatus::Malformed;
            result.action = AuthAction::Assignment;
            break;
        }
        default:
            break;
        }
        out = std::move(result);
        return DecodeStatus::Complete;
    }
    bool SessionStatus(uint64_t connection, Buffer& out) const override {
        Buffer result;
        if (!result.U16(2) || !result.VarU32(static_cast<uint32_t>(connection)))
            return false;
        out = std::move(result);
        return true;
    }
    bool Reply(const AuthMessage& message, Buffer& out) const override {
        if (message.id != 12 && message.id != 14)
            return false;
        Buffer result;
        if (!Ack(message.request, result))
            return false;
        out = std::move(result);
        return true;
    }
    bool Deletion(uint32_t request, Buffer& out) const override {
        Buffer result;
        if (!Ack(request, result))
            return false;
        out = std::move(result);
        return true;
    }
    bool Selection(const AccountState* account, uint32_t request, uint32_t seconds, Buffer& out) const override {
        Buffer result;
        if (account && !china167409::EncodeAccount(*account, request, uint64_t(seconds) + 946684800ULL, result))
            return false;
        if (!Ack(request, result, account ? 1002 : 0, account ? 3 : 0, account ? 3516 : 0))
            return false;
        out = std::move(result);
        return true;
    }
    bool CharacterList(const AccountState& account, uint32_t request, std::vector<Buffer>& out) const override {
        std::vector<Buffer> result;
        Buffer start, roster;
        if (!start.U16(23) || !start.U16(24) || !china167409::EncodeRoster(account, request, roster) ||
            !Ack(request, roster))
            return false;
        result.push_back(std::move(start));
        result.push_back(std::move(roster));
        out = std::move(result);
        return true;
    }
    bool Assignment(const AuthMessage& message, uint32_t association, uint32_t map, uint32_t nonce,
                    Buffer& out) const override {
        return Build204489Auth().Assignment(message, association, map, nonce, out);
    }
    bool Login(const AccountState& account, uint32_t, uint32_t request, uint32_t, uint32_t seconds,
               std::vector<Buffer>& out) const override {
        // Only the initial account reply. The required profile AuthFlow waits for C4 before completing login.
        Buffer accountReply;
        if (!china167409::EncodeAccount(account, request, uint64_t(seconds) + 946684800ULL, accountReply))
            return false;
        std::vector<Buffer> result;
        result.push_back(std::move(accountReply));
        out = std::move(result);
        return true;
    }
};
} // namespace
const AuthCodec& China167409Auth() {
    static const ChinaAuth auth;
    return auth;
}
} // namespace glide
