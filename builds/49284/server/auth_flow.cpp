#include "auth_flow.h"
#include <algorithm>
#include <ctime>

namespace glide {
namespace hot49284 {
namespace {
class HotAuthFlow final : public glide::AuthFlow {
  public:
    CharacterCatalog catalog;
    std::function<uint64_t()> clock;
    uint32_t base = 0;
    DecodeStatus Step(ByteView input, AuthServices& services, size_t& consumed, std::vector<Buffer>& frames,
                      std::string& error) override {
        AuthRequest request;
        const auto status = DecodeAuthRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        std::vector<Buffer> result;
        error = "HoT Auth request is not valid in the current login state";
        if (!Handle(request, services, result, error))
            return DecodeStatus::Malformed;
        consumed = request.consumed;
        frames = std::move(result);
        error.clear();
        return DecodeStatus::Complete;
    }

  private:
    bool Snapshot(const AccountState& saved, uint32_t request, AccountSnapshot& out, std::vector<RosterEntry>& roster,
                  std::string& error) {
        AccountSnapshot account;
        account.request = request;
        account.type = 2;
        // Retained local fresh-account policy: five slots, own unencrypted packs.
        account.homeWorld = 1001;
        account.worldClass = WorldClass(account.homeWorld);
        account.authBaseSeconds = base;
        AccountCapability slots;
        slots.id = 2;
        slots.value = 1;
        account.capabilities.push_back(slots);
        std::copy(saved.userGuid.bytes, saved.userGuid.bytes + 16, account.user.begin());
        if (std::all_of(account.user.begin(), account.user.end(), [](uint8_t b) { return b == 0; }) ||
            !AuthTime(clock(), account.serverSeconds) || saved.characters.size() > 5)
            return false;
        const auto count = saved.document.Count("", "agreement_version");
        if (count) {
            std::string value;
            unsigned agreement = 0;
            if (count != 1 || !saved.document.Get("", "agreement_version", value) ||
                !ParseUnsigned(value, 255, agreement))
                return false;
            account.agreement = agreement;
        }
        bool selected = saved.characters.empty();
        for (const auto& character : saved.characters) {
            RosterEntry entry;
            if (!catalog.Resolve(character, entry, error))
                return false;
            if (entry.name.empty() ||
                std::all_of(entry.guid.begin(), entry.guid.end(), [](uint8_t b) { return b == 0; }))
                return false;
            for (const auto& previous : roster)
                if (previous.guid == entry.guid || previous.name == entry.name)
                    return false;
            if (character.slot == saved.selected) {
                if (selected)
                    return false;
                account.selected = entry.guid;
                selected = true;
            }
            roster.push_back(std::move(entry));
        }
        if (!selected)
            return false;
        out = std::move(account);
        return true;
    }
    bool Handle(const AuthRequest& request, AuthServices& services, std::vector<Buffer>& frames, std::string& error) {
        if (loggedOut_)
            return false;
        Buffer reply;
        switch (request.operation) {
        case AuthOperation::Clock:
        case AuthOperation::Machine:
            return true;
        case AuthOperation::Identity: {
            const auto id = services.ConnectionId();
            if (!id || id > UINT32_MAX || !reply.U16(2) || !reply.VarU32(static_cast<uint32_t>(id)))
                return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Login: {
            if (started_ || !request.request || !services.Claim(request.identity))
                return false;
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            Buffer config;
            if (!services.Load(saved, error) || !ReadConfig(saved.document, config) ||
                !Snapshot(saved, request.request, account, roster, error) ||
                !login_.Begin(account, roster, config.View(), frames))
                return false;
            started_ = true;
            return true;
        }
        case AuthOperation::Content:
            if (!started_ || ready_ || !services.Active() || !login_.Pending() || !login_.ContentReported(frames))
                return false;
            ready_ = true;
            return true;
        default:
            break;
        }
        if (!ready_ || !services.Active() || login_.Pending())
            return false;
        switch (request.operation) {
        case AuthOperation::Config:
            if (!CommitConfig(request, [&](ByteView bytes) { return services.SaveConfig(bytes, error); }, reply))
                return false;
            frames.push_back(std::move(reply));
            return true;
        case AuthOperation::Agreement:
            return services.Agreement(request.agreement, error);
        case AuthOperation::SystemReport:
            return request.request <= 1;
        case AuthOperation::Redirect: {
            const auto port = services.Port();
            if (!port || !ValidateRedirect(request, previousReferral_))
                return false;
            uint32_t association = 0, map = 0;
            if (!services.Refer(request.routing[0], request.routing[2], request.routing[3], 5, association, map, error))
                return false;
            if (!RedirectReply(request, association, map, port, reply)) {
                services.RevokePending();
                return false;
            }
            previousReferral_ = request.request;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::CurrentWorld:
            // Retained local HoT policy already hosts advertised world1001.
            if (!request.request || request.world != 1001 || request.temporary > 1 || !reply.U16(4) ||
                !reply.VarU32(request.request))
                return false;
            for (unsigned i = 0; i < 4; ++i)
                if (!reply.VarU32(0))
                    return false;
            frames.push_back(std::move(reply));
            return true;
        case AuthOperation::ViewLeave:
            return services.RevokePending();
        case AuthOperation::Logout:
            if (!services.RevokePending())
                return false;
            loggedOut_ = true;
            return true;
        case AuthOperation::Roster: {
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            if (!request.request || !services.Load(saved, error) ||
                !Snapshot(saved, request.request, account, roster, error))
                return false;
            for (const auto& entry : roster) {
                Buffer encoded;
                if (!CharacterEntry(entry, request.request, encoded) || !reply.Append(encoded.View()))
                    return false;
            }
            if (!reply.U16(4) || !reply.VarU32(request.request))
                return false;
            for (unsigned i = 0; i < 4; ++i)
                if (!reply.VarU32(0))
                    return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Delete: {
            if (!request.request || request.name.empty())
                return false;
            // C20 uses HoT's existing S4 transaction completion. Prepare it
            // before storage changes and publish only after durable deletion.
            if (!reply.U16(4) || !reply.VarU32(request.request))
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
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            const auto port = services.Port();
            if (!port || !request.request || request.name.empty() || !services.Select(request.name, saved, error) ||
                !Snapshot(saved, request.request, account, roster, error))
                return false;
            const RosterEntry* selected = nullptr;
            for (const auto& entry : roster)
                if (entry.guid == account.selected)
                    selected = &entry;
            if (!selected || selected->name != request.name || !selected->summary.map || !EncodeAccount(account, reply))
                return false;
            // C22/S3 endpoint uses the configured local listener port.
            uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
            endpoint[2] = static_cast<uint8_t>(port >> 8);
            endpoint[3] = static_cast<uint8_t>(port);
            if (!reply.U16(3) || !reply.VarU32(request.request) || !reply.VarU32(selected->summary.map) ||
                !reply.Append(ByteView(endpoint, sizeof(endpoint))))
                return false;
            for (unsigned i = 0; i < 4; ++i)
                if (!reply.VarU32(0))
                    return false;
            frames.push_back(std::move(reply));
            return true;
        }
        default:
            // Never fall through to another build's codec.
            return false;
        }
    }
    LoginReplies login_;
    bool started_ = false, ready_ = false, loggedOut_ = false;
    uint32_t previousReferral_ = 0;
};
} // namespace
bool CreateAuthFlow(CharacterCatalog catalog, std::function<uint64_t()> clock, std::unique_ptr<glide::AuthFlow>& out,
                    std::string& error) {
    error = "HoT Auth requires its character recipes and a valid epoch-2001 clock";
    uint32_t base = 0;
    if (catalog.RecipeCount() != 54 || !clock || !AuthTime(clock(), base))
        return false;
    std::unique_ptr<HotAuthFlow> flow(new HotAuthFlow);
    flow->catalog = std::move(catalog);
    flow->clock = std::move(clock);
    flow->base = base;
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateAuthFlow(const std::wstring& dataRoot, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    CharacterCatalog catalog;
    if (!catalog.Load(dataRoot + L"/49284/construction.bin", error))
        return false;
    return CreateAuthFlow(
        std::move(catalog),
        []() {
            const auto now = std::time(nullptr);
            return now > 0 ? static_cast<uint64_t>(now) : uint64_t(0);
        },
        out, error);
}
} // namespace hot49284
} // namespace glide
