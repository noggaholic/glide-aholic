#include "auth_flow.h"
#include "auth_request.h"
#include "auth_route.h"
#include "glide/files.h"
#include <algorithm>
namespace glide {
namespace july14812 {
namespace {
class JulyAuthFlow final : public glide::AuthFlow {
  public:
    CharacterCatalog catalog;
    std::array<uint8_t, 32> contentKey{};
    DecodeStatus Step(ByteView input, AuthServices& services, size_t& consumed, std::vector<Buffer>& frames,
                      std::string& error) override {
        AuthRequest request;
        const auto status = DecodeAuthRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        std::vector<Buffer> result;
        error = "July Auth request is not valid in the current login state";
        if (!Handle(request, services, result, error)) {
            if (request.operation == AuthOperation::Redirect)
                error = "July redirect rejected: route=" + std::to_string(request.routing[0]) +
                        " association=" + std::to_string(request.routing[1]) +
                        " map=" + std::to_string(request.routing[2]) +
                        " context=" + std::to_string(request.routing[3]) + "; " + error;
            return DecodeStatus::Malformed;
        }
        consumed = request.consumed;
        frames = std::move(result);
        error.clear();
        return DecodeStatus::Complete;
    }

  private:
    bool Snapshot(const AccountState& saved, uint32_t request, AccountSnapshot& out, std::vector<RosterEntry>& roster,
                  std::string& error) const {
        AccountSnapshot account;
        account.request = request;
        account.type = 2;
        account.homeWorld = 1001;
        account.worldClass = 1;
        account.contentKey = contentKey;
        std::copy(saved.userGuid.bytes, saved.userGuid.bytes + 16, account.user.begin());
        if (!request || std::all_of(account.user.begin(), account.user.end(), [](uint8_t b) { return b == 0; }) ||
            saved.characters.size() > 5 || (started_ && account.user != user_))
            return false;
        const auto count = saved.document.Count("", "agreement_version");
        if (count) {
            std::string value;
            unsigned version = 0;
            if (count != 1 || !saved.document.Get("", "agreement_version", value) ||
                !ParseUnsigned(value, 255, version))
                return false;
            account.agreement = version;
        }
        bool selected = saved.characters.empty();
        for (const auto& character : saved.characters) {
            RosterEntry entry;
            if (!catalog.Resolve(character, entry, error) ||
                std::all_of(entry.guid.begin(), entry.guid.end(), [](uint8_t b) { return b == 0; }))
                return false;
            for (const auto& before : roster)
                if (before.guid == entry.guid || before.name == entry.name)
                    return false;
            if (character.slot == saved.selected) {
                if (selected)
                    return false;
                selected = true;
                account.selected = entry.guid;
            }
            roster.push_back(std::move(entry));
        }
        if (!selected)
            return false;
        out = std::move(account);
        return true;
    }
    bool Handle(const AuthRequest& request, AuthServices& services, std::vector<Buffer>& frames, std::string& error) {
        Buffer reply;
        switch (request.operation) {
        case AuthOperation::Clock:
        case AuthOperation::Machine:
        case AuthOperation::Reauthenticate:
        case AuthOperation::Sequence:
            return true;
        case AuthOperation::Identity: {
            const auto id = services.ConnectionId();
            if (!id || id > UINT32_MAX || !reply.U16(2) || !reply.VarU32(static_cast<uint32_t>(id)))
                return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Login: {
            if (!request.request || login_.Pending() || config_.Active())
                return false;
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            Buffer preferences;
            if (!services.PrepareLogin(request.identity, saved, error) ||
                !Snapshot(saved, request.request, account, roster, error) || !ReadConfig(saved.document, preferences) ||
                !login_.Begin(account, roster, preferences.View(), frames,
                              [&]() { return services.ClaimPrepared(request.identity); }))
                return false;
            user_ = account.user;
            started_ = true;
            ready_ = !login_.Pending();
            previousReferral_ = 0;
            return true;
        }
        case AuthOperation::Content:
            if (!started_ || !services.Active() || !login_.ContentReported(frames))
                return false;
            ready_ = true;
            return true;
        default:
            break;
        }
        if (!ready_ || !services.Active() || login_.Pending())
            return false;
        switch (request.operation) {
        case AuthOperation::ConfigBegin:
            return config_.Begin(request.request, request.totalBytes);
        case AuthOperation::ConfigChunk:
            if (!config_.Chunk(
                    request.request, request.data, [&](ByteView bytes) { return services.SaveConfig(bytes, error); },
                    reply))
                return false;
            if (reply.Size())
                frames.push_back(std::move(reply));
            return true;
        case AuthOperation::Agreement:
            return services.Agreement(request.agreement, error);
        case AuthOperation::Delete: {
            if (!request.request || request.name.empty() || config_.Active())
                return false;
            // Use July's own completion and publish success only after saved deletion.
            if (!Completion(request.request, reply) || !services.Delete(request.name, error))
                return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Select: {
            const auto port = services.Port();
            if (!port || !request.request || request.name.empty() || config_.Active())
                return false;
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            if (!services.Load(saved, error) || !Snapshot(saved, request.request, account, roster, error))
                return false;
            const RosterEntry* chosen = nullptr;
            for (const auto& entry : roster)
                if (entry.name == request.name)
                    chosen = &entry;
            if (!chosen)
                return false;
            account.selected = chosen->guid;
            SelectResult selected;
            selected.request = request.request;
            selected.map = chosen->summary.map;
            Buffer route;
            if (!LoopbackEndpoint(port, selected.endpoint) || !EncodeAccount(account, reply) ||
                !EncodeSelect(selected, route) || !reply.Append(route.View()))
                return false;
            AccountState committed;
            if (!services.Select(request.name, committed, error))
                return false;
            AccountSnapshot actual;
            std::vector<RosterEntry> checked;
            if (!Snapshot(committed, request.request, actual, checked, error) || actual.selected != account.selected ||
                !services.RevokePending())
                return false;
            for (const auto& entry : checked)
                if (entry.guid == actual.selected &&
                    (entry.name != chosen->name || entry.summary.map != chosen->summary.map))
                    return false;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Redirect: {
            const auto port = services.Port();
            const auto& routing = request.routing;
            if (!port || !request.request || request.request == previousReferral_ || config_.Active() || routing[1] ||
                (routing[0] != 1 && routing[0] != 3) || (routing[0] == 1 && (routing[2] || routing[3])))
                return false;
            uint32_t association = 0, map = 0;
            if (!services.Refer(routing[0], routing[2], routing[3], 5, association, map, error))
                return false;
            RedirectResult result;
            result.request = request.request;
            result.routing = {{routing[0], association, map, routing[3]}};
            if (!association ||
                (routing[0] == 1 ? map != 0 : (!map || map > UINT16_MAX || (routing[2] && routing[2] != map))) ||
                !LoopbackEndpoint(port, result.endpoint) || !EncodeRedirect(result, reply)) {
                services.RevokePending();
                return false;
            }
            previousReferral_ = request.request;
            frames.push_back(std::move(reply));
            return true;
        }
        case AuthOperation::Roster: {
            if (!request.request || config_.Active())
                return false;
            AccountState saved;
            AccountSnapshot account;
            std::vector<RosterEntry> roster;
            if (!services.Load(saved, error) || !Snapshot(saved, request.request, account, roster, error))
                return false;
            for (const auto& entry : roster) {
                Buffer encoded;
                if (!CharacterEntry(entry, request.request, encoded) || !reply.Append(encoded.View()))
                    return false;
            }
            Buffer ack;
            if (!Completion(request.request, ack) || !reply.Append(ack.View()) || !services.RevokePending())
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
    bool started_ = false, ready_ = false;
    std::array<uint8_t, 16> user_{};
    uint32_t previousReferral_ = 0;
};
} // namespace
bool CreateAuthFlow(CharacterCatalog catalog, const std::array<uint8_t, 32>& key, std::unique_ptr<glide::AuthFlow>& out,
                    std::string& error) {
    if (catalog.RecipeCount() != 48 || catalog.Palette().size() != 391) {
        error = "July Auth requires its complete recipe matrix";
        return false;
    }
    std::unique_ptr<JulyAuthFlow> flow(new JulyAuthFlow);
    flow->catalog = std::move(catalog);
    flow->contentKey = key;
    out = std::move(flow);
    error.clear();
    return true;
}
bool CreateAuthFlow(const std::wstring& root, std::unique_ptr<glide::AuthFlow>& out, std::string& error) {
    CharacterCatalog catalog;
    Buffer key;
    if (!catalog.Load(root + L"/14812/construction.bin", error))
        return false;
    if (!ReadFileBytes(root + L"/14812/content-seed.bin", 32, key) || key.Size() != 32) {
        error = "July content seed is missing or malformed";
        return false;
    }
    std::array<uint8_t, 32> owned;
    std::copy(key.Data(), key.Data() + 32, owned.begin());
    return CreateAuthFlow(std::move(catalog), owned, out, error);
}
} // namespace july14812
} // namespace glide
