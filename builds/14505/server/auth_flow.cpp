#include "auth_flow.h"
#include "auth_request.h"
#include "character_catalog.h"
#include "../../14075/server/auth_request.h"
#include "../../14075/server/auth_response.h"

namespace glide {
namespace june14505 {
namespace {
// Retained June account-content seed, shared by the old local content provider.
const uint8_t seed[32] = {0x0f, 0x8a, 0xa1, 0xbb, 0x47, 0x79, 0xb8, 0xf9, 0x20, 0xec, 0x40,
                          0xfa, 0x45, 0x2c, 0x7d, 0xed, 0x0c, 0xa2, 0xe4, 0x99, 0x70, 0x7c,
                          0xe9, 0x30, 0x6c, 0x13, 0x01, 0xc5, 0x80, 0xff, 0x57, 0x31};
class BetaAuthFlow : public glide::AuthFlow {
  public:
    explicit BetaAuthFlow(unsigned build) : catalog(build) {}
    CharacterCatalog catalog;
    DecodeStatus Step(ByteView input, AuthServices& services, size_t& consumed, std::vector<Buffer>& frames,
                      std::string& error) override {
        AuthRequest message;
        const auto status = April() ? april14075::DecodeAuthRequest(input, message) : DecodeAuthRequest(input, message);
        if (status != DecodeStatus::Complete)
            return status;
        std::vector<Buffer> result;
        error = "Beta Auth request is not valid in the current login state";
        if (!Handle(message, services, result, error))
            return DecodeStatus::Malformed;
        consumed = message.consumed;
        frames = std::move(result);
        error.clear();
        return DecodeStatus::Complete;
    }

  private:
    bool April() const {
        return catalog.Build() == 14075;
    }
    bool State(const AccountState& account, const Guid& selected, uint32_t request, Buffer& out) const {
        return April() ? april14075::AccountReply(account, selected, request, ByteView(seed, sizeof(seed)), out)
                       : AccountReply(account, selected, request, ByteView(seed, sizeof(seed)), out);
    }
    bool BeginLogin(const AccountState& account, const std::vector<RosterCharacter>& roster, uint32_t request,
                    ByteView preferences, std::vector<Buffer>& frames) {
        if (!April())
            return login_.Begin(account, roster, request, ByteView(seed, sizeof(seed)), frames, preferences);
        Buffer state;
        std::vector<Buffer> completion;
        return State(account, account.userGuid, request, state) &&
               april14075::LoginComplete(roster, request, completion, preferences) &&
               login_.BeginPrepared(std::move(state), std::move(completion), frames);
    }
    bool Handle(const AuthRequest& request, AuthServices& services, std::vector<Buffer>& frames, std::string& error) {
        Buffer reply;
        switch (request.operation) {
        case AuthOperation::Clock:
        case AuthOperation::Machine:
        case AuthOperation::Sequence:
        case AuthOperation::Reauthenticate:
            return true;
        case AuthOperation::Identity:
            if (!(April() ? april14075::SessionReady(static_cast<uint32_t>(services.ConnectionId()), reply)
                          : SessionReady(static_cast<uint32_t>(services.ConnectionId()), reply)))
                return false;
            frames.push_back(std::move(reply));
            return true;
        case AuthOperation::Login: {
            if (!request.request || login_.Pending() || config_.Active() || !services.Claim(request.identity))
                return false;
            AccountState account;
            Buffer preferences;
            if (!services.Load(account, error) || !ReadConfig(account.document, preferences))
                return false;
            std::vector<RosterCharacter> roster;
            for (const auto& saved : account.characters) {
                RosterCharacter character;
                if (!catalog.Resolve(saved, character, error))
                    return false;
                roster.push_back(std::move(character));
            }
            if (!BeginLogin(account, roster, request.request, preferences.View(), frames))
                return false;
            ready_ = !login_.Pending();
            return true;
        }
        case AuthOperation::Content:
            if (!services.Active() || !login_.Pending() || !login_.ContentReported(frames))
                return false;
            ready_ = true;
            return true;
        default:
            break;
        }
        if (!ready_ || !services.Active() || login_.Pending())
            return false;
        switch (request.operation) {
        case AuthOperation::LegacyControl:
            return April(); // It cannot issue a game referral.
        case AuthOperation::ConfigBegin:
            return config_.Begin(request.request, request.totalBytes);
        case AuthOperation::ConfigChunk:
            if (!config_.Chunk(request.request, request.data))
                return false;
            if (!config_.Ready())
                return true;
            if (!config_.Finish([&](ByteView bytes) { return services.SaveConfig(bytes, error); }, reply))
                return false;
            if (April()) {
                reply.Data()[0] = 0x1b;
                reply.Data()[1] = 0;
            }
            frames.push_back(std::move(reply));
            return true;
        case AuthOperation::Agreement:
            return services.Agreement(request.agreement, error);
        case AuthOperation::Delete: {
            if (!request.request || request.name.empty() || config_.Active())
                return false;
            // The native delete transaction consumes the same S4 completion as login.
            // Prepare the response before saving; publish success only after durable deletion.
            if (!reply.U16(April() ? 3 : 4) || !reply.VarU32(request.request))
                return false;
            for (unsigned i = 0; i < 4; ++i)
                if (!reply.VarU32(0))
                    return false;
            if (!services.Delete(request.name, error))
                return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Select: {
            if (!request.request || config_.Active())
                return false;
            AccountState account;
            if (!services.Select(request.name, account, error))
                return false;
            const Character* selected = nullptr;
            for (const auto& character : account.characters)
                if (character.slot == account.selected)
                    selected = &character;
            RosterCharacter checked;
            if (!selected || !catalog.Resolve(*selected, checked, error))
                return false;
            Buffer state;
            if (!State(account, selected->guid, request.request, state) ||
                !(April() ? april14075::SelectionReply(request.request, selected->map, reply)
                          : SelectionReply(request.request, selected->map, reply)) ||
                !state.Append(reply.View()))
                return false;
            frames.push_back(std::move(state));
            return true;
        }
        case AuthOperation::Redirect: {
            if (!request.request || config_.Active())
                return false;
            uint32_t association = 0, map = 0, nonce = 0;
            if (!services.Assign(request.routing[0], association, map, nonce, error))
                return false;
            const uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
            // The account service issues routing values; never echo client-supplied permits.
            if (!reply.U16(April() ? 9 : 20) || !reply.VarU32(request.request) || !reply.VarU32(request.routing[0]) ||
                !reply.VarU32(association) || !reply.VarU32(map) ||
                !reply.Append(ByteView(endpoint, sizeof(endpoint))) || !reply.VarU32(nonce))
                return false;
            frames.push_back(std::move(reply));
            return true;
        }
        default:
            return false;
        }
    }
    LoginReplies login_;
    ConfigUpload config_;
    bool ready_ = false;
};
} // namespace
bool CreateAuthFlowFromCatalog(CharacterCatalog catalog, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    if ((catalog.Build() != 14422 && catalog.Build() != 14505) || !catalog.RecipeCount() || !catalog.ColorCount()) {
        error = "Missing or unsupported June character catalog";
        return false;
    }
    return CreateBetaAuthFlowFromCatalog(std::move(catalog), out, error);
}
bool CreateBetaAuthFlowFromCatalog(CharacterCatalog catalog, std::unique_ptr<glide::AuthFlow>& out,
                                   std::string& error) {
    if ((catalog.Build() != 14075 && catalog.Build() != 14422 && catalog.Build() != 14505) || !catalog.RecipeCount() ||
        !catalog.ColorCount()) {
        error = "Missing or unsupported beta character catalog";
        return false;
    }
    std::unique_ptr<BetaAuthFlow> flow(new BetaAuthFlow(catalog.Build()));
    flow->catalog = std::move(catalog);
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.Load(dataRoot + L"/14505/character-recipes.txt", error) ||
        !catalog.LoadWeapons(dataRoot + L"/14505/weapon-loadouts.txt", error))
        return false;
    return CreateAuthFlowFromCatalog(std::move(catalog), out, error);
}
} // namespace june14505
} // namespace glide
