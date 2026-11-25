#include "server.h"
#include "glide/crypto.h"
#include "glide/guid.h"
#include "glide/srp.h"
#include <cstring>

namespace glide {
namespace {
bool Token(std::string& out) {
    Guid token;
    if (!RandomBytes(token.bytes, sizeof(token.bytes)))
        return false;
    out = token.Text();
    return true;
}
class AccountLookup : public TlsSrpAccounts {
  public:
    std::function<bool(const std::string&, Buffer&, Buffer&)> lookup;
    bool Lookup(const std::string& identity, Buffer& salt, Buffer& verifier) override {
        return lookup(identity, salt, verifier);
    }
};
} // namespace
std::shared_ptr<const LoginSession> Server::Login(uint64_t owner, const std::string& email,
                                                  const std::string& password) {
    std::string login;
    if (!NormalizeLogin(email, login))
        return nullptr;
    AccountOperation accountOperation(*this, login);
    StateGuard guard(*this);
    Prune();
    if (!accounts_)
        return nullptr;
    std::string stored, guid, display, error;
    KvDocument account;
    if (!LoadAccountLocked(login, account, error) || !account.Get("", "password", stored) || stored != password ||
        !account.Get("", "user_guid", guid) || !account.Get("", "display_name", display))
        return nullptr;
    return IssueLoginLocked(owner, login, guid, display);
}
std::shared_ptr<const LoginSession> Server::LoginSrp(uint64_t owner, TlsSession& tls, ByteView initial,
                                                     std::string& error, TlsEntropy saltEntropy) {
    std::string login, identity, guid, display;
    AccountLookup provider;
    provider.lookup = [&](const std::string& name, Buffer& salt, Buffer& verifier) {
        std::string password, loadError;
        if (!NormalizeLogin(name, login))
            return false;
        AccountOperation accountOperation(*this, login);
        {
            StateGuard guard(*this);
            KvDocument account;
            if (!accounts_ || !LoadAccountLocked(login, account, loadError) || !account.Get("", "password", password) ||
                !account.Get("", "user_guid", guid) || !account.Get("", "display_name", display))
                return false;
        }
        uint8_t bytes[16];
        const bool random = saltEntropy ? saltEntropy(bytes, sizeof(bytes)) : RandomBytes(bytes, sizeof(bytes));
        // File names are normalized, but SRP hashes the identity supplied by the client.
        const bool ready = random && salt.Append(ByteView(bytes, sizeof(bytes))) &&
                           Srp1024Verifier(ByteView(name.data(), name.size()),
                                           ByteView(password.data(), password.size()), salt.View(), verifier);
        if (!password.empty())
            SecureZeroMemory(&password[0], password.size());
        if (ready)
            identity = name;
        return ready;
    };
    if (!owner || !tls.AcceptSrp(initial, provider, error) || !tls.PasswordVerified() || identity.empty() ||
        tls.LoginName() != identity)
        return nullptr;
    AccountOperation accountOperation(*this, login);
    StateGuard guard(*this);
    Prune();
    const auto session = IssueLoginLocked(owner, login, guid, display);
    if (!session)
        error = "Account cannot start another session";
    return session;
}
std::shared_ptr<const LoginSession> Server::IssueLoginLocked(uint64_t owner, const std::string& login,
                                                             const std::string& guid, const std::string& display) {
    if (!owner)
        return nullptr;
    const auto existing = sessions_.find(login);
    if (existing != sessions_.end() && !existing->second.owners.empty())
        return nullptr;
    std::shared_ptr<LoginSession> session(new LoginSession);
    Guid user;
    if (!Guid::Parse(guid, user))
        return nullptr;
    session->login = login;
    session->userId = user.Text();
    session->displayName = display;
    if (!Token(session->resumeToken))
        return nullptr;
    SessionState state;
    state.session = session;
    state.owners.insert(owner);
    state.expires = clock_() + 300000;
    sessions_[login] = std::move(state);
    return session;
}
std::shared_ptr<const LoginSession> Server::Resume(uint64_t owner, const std::string& token) {
    StateGuard guard(*this);
    Prune();
    if (token.empty())
        return nullptr;
    for (auto& pair : sessions_)
        if (!pair.second.loggedOut && pair.second.session->resumeToken == token) {
            pair.second.owners.insert(owner);
            return pair.second.session;
        }
    return nullptr;
}
bool Server::SessionActive(const std::shared_ptr<const LoginSession>& session) {
    StateGuard guard(*this);
    if (!session)
        return false;
    const auto at = sessions_.find(session->login);
    return at != sessions_.end() && at->second.session == session && !at->second.loggedOut;
}
bool Server::Logout(uint64_t owner, const std::shared_ptr<const LoginSession>& session) {
    if (!owner || !session)
        return false;
    AccountOperation accountOperation(*this, session->login);
    std::vector<std::shared_ptr<Socket>> sockets;
    {
        StateGuard guard(*this);
        const auto at = sessions_.find(session->login);
        if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
            !at->second.owners.count(owner))
            return false;
        auto& state = at->second;
        LeavePeersLocked(state);
        state.loggedOut = true;
        state.gameToken.clear();
        state.createAssociation = 0;
        state.worldEntry = WorldEntry();
        // Keep the world worker's ownership until its final position save.
        // The requesting STS socket stays open to receive the logout reply.
        state.owners.erase(owner);
        for (const auto other : state.owners) {
            const auto socket = ownerSockets_.find(other);
            if (socket != ownerSockets_.end())
                sockets.push_back(socket->second);
        }
        state.owners.clear();
        if (state.worldOwner)
            state.owners.insert(state.worldOwner);
        Prune();
    }
    for (const auto& socket : sockets)
        socket->Shutdown();
    return true;
}
bool Server::IssueGameToken(const std::shared_ptr<const LoginSession>& session, std::string& token) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    if (!session)
        return false;
    auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut || at->second.owners.empty())
        return false;
    std::string issued;
    if (!Token(issued))
        return false;
    at->second.gameToken = issued;
    at->second.tokenExpires = clock_() + 60000;
    token = std::move(issued);
    return true;
}
std::shared_ptr<const LoginSession> Server::ClaimGameToken(uint64_t owner, const std::string& token) {
    StateGuard guard(*this);
    Prune();
    if (token.empty() || pendingAdmissions_.count(owner))
        return nullptr;
    for (auto& pair : sessions_) {
        auto& state = pair.second;
        if (!state.loggedOut && state.gameToken == token && state.tokenExpires > clock_()) {
            state.gameToken.clear();
            state.owners.insert(owner);
            return state.session;
        }
    }
    return nullptr;
}
Server::SessionState* Server::FindPendingTokenIdentityLocked(ByteView identity) {
    if (!identity.Valid() || identity.size != 32)
        return nullptr;
    SessionState* match = nullptr;
    const auto now = clock_();
    for (auto& pair : sessions_) {
        auto& state = pair.second;
        if (state.loggedOut || state.gameToken.empty() || state.tokenExpires <= now)
            continue;
        Guid token;
        if (!Guid::Parse(state.gameToken, token) ||
            (!token.Matches(ByteView(identity.data, 16)) && !token.Matches(ByteView(identity.data + 16, 16))))
            continue;
        if (match)
            return nullptr;
        match = &state;
    }
    return match;
}
bool Server::PrepareTokenAuth(ByteView identity, std::shared_ptr<const LoginSession>& prepared, AccountState& out,
                              std::string& error) {
    std::shared_ptr<const LoginSession> candidate;
    {
        StateGuard guard(*this);
        Prune();
        const auto* match = FindPendingTokenIdentityLocked(identity);
        if (match)
            candidate = match->session;
    }
    if (!candidate) {
        error = "No unique pending game token";
        return false;
    }
    AccountOperation accountOperation(*this, candidate->login);
    StateGuard guard(*this);
    Prune();
    const auto* match = FindPendingTokenIdentityLocked(identity);
    if (!match || match->session != candidate || !accounts_) {
        error = "No unique pending game token";
        return false;
    }
    KvDocument document;
    AccountState account;
    if (!LoadAccountLocked(candidate->login, document, error) || !DecodeAccountState(document, account, error))
        return false;
    match = FindPendingTokenIdentityLocked(identity);
    if (!match || match->session != candidate) {
        error = "Game token changed during account read";
        return false;
    }
    auto session = match->session;
    out = std::move(account);
    prepared = std::move(session);
    error.clear();
    return true;
}
std::shared_ptr<const LoginSession>
Server::ClaimTokenAuthIdentity(uint64_t owner, ByteView identity, const std::shared_ptr<const LoginSession>& prepared) {
    if (!owner || !prepared)
        return nullptr;
    StateGuard guard(*this);
    Prune();
    if (pendingAdmissions_.count(owner))
        return nullptr;
    auto* match = FindPendingTokenIdentityLocked(identity);
    if (!match || match->session != prepared)
        return nullptr;
    match->gameToken.clear();
    match->owners.insert(owner);
    return match->session;
}
std::shared_ptr<const LoginSession> Server::ClaimAuthIdentity(uint64_t owner, ByteView identity) {
    if (!identity.Valid() || identity.size != 32)
        return nullptr;
    StateGuard guard(*this);
    Prune();
    if (pendingAdmissions_.count(owner))
        return nullptr;
    SessionState* match = nullptr;
    const uint64_t now = clock_();
    for (auto& pair : sessions_) {
        auto& state = pair.second;
        if (state.loggedOut || state.gameToken.empty() || state.tokenExpires <= now)
            continue;
        bool matches = false;
        const std::string* candidates[] = {&state.session->userId, &state.session->resumeToken, &state.gameToken};
        for (const auto* text : candidates) {
            Guid guid;
            if (Guid::Parse(*text, guid) &&
                (guid.Matches(ByteView(identity.data, 16)) || guid.Matches(ByteView(identity.data + 16, 16))))
                matches = true;
        }
        if (!matches)
            continue;
        if (match)
            return nullptr; // An ambiguous identity never consumes either ticket.
        match = &state;
    }
    if (!match)
        return nullptr;
    match->gameToken.clear();
    match->owners.insert(owner);
    return match->session;
}
bool Server::LoadState(const std::shared_ptr<const LoginSession>& session, AccountState& out, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    if (!session || !accounts_)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut)
        return false;
    KvDocument document;
    return LoadAccountLocked(session->login, document, error) && DecodeAccountState(document, out, error);
}
SelectResult Server::SelectCharacter(const std::shared_ptr<const LoginSession>& session, const std::string& name,
                                     AccountState& out, std::string& error) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    error = "Cannot select character for this session";
    if (!session || !accounts_ || !profile_)
        return SelectResult::Failed;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut || at->second.owners.empty())
        return SelectResult::Failed;
    if (at->second.worldOwner)
        return SelectResult::Failed;
    std::wstring requested;
    if (!Utf8ToWide(name, requested) || requested.empty()) {
        error.clear();
        return SelectResult::NotFound;
    }
    KvDocument document;
    AccountState state;
    if (!LoadAccountLocked(session->login, document, error) || !DecodeAccountState(document, state, error))
        return SelectResult::Failed;
    for (const auto& character : state.characters) {
        std::wstring candidate;
        if (!Utf8ToWide(character.name, candidate))
            return SelectResult::Failed;
        if (CompareStringOrdinal(requested.c_str(), -1, candidate.c_str(), -1, TRUE) != CSTR_EQUAL)
            continue;
        // Native creation confirms the already selected character through the Auth connection.
        // Changing to a different character must still wait until creation closes.
        if (at->second.createOwner && state.selected != character.slot)
            return SelectResult::Failed;
        if (!catalog_.Available(character.map)) {
            error = "Saved character map is unavailable";
            return SelectResult::Failed;
        }
        Buffer summary;
        const CharacterCodec& codec =
            characterCodec_ ? *characterCodec_ : static_cast<const CharacterCodec&>(profile_->Auth());
        if (!codec.Summary(character, summary)) {
            error = "Invalid saved character appearance";
            return SelectResult::Failed;
        }
        if (state.selected != character.slot) {
            if (!state.document.Set("", "selected", std::to_string(character.slot)) ||
                !SaveAccountLocked(state.document, error))
                return SelectResult::Failed;
            state.selected = character.slot;
        }
        if (!at->second.createOwner)
            at->second.worldEntry.association = 0;
        if (at->second.createOwner && at->second.createSaved &&
            !std::memcmp(at->second.createdCharacter.bytes, character.guid.bytes, 16))
            at->second.createConfirmed = character.guid;
        out = std::move(state);
        error.clear();
        return SelectResult::Selected;
    }
    error.clear();
    return SelectResult::NotFound;
}
bool Server::RevokePending(const std::shared_ptr<const LoginSession>& session, uint64_t issuer) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    if (!session || !issuer)
        return false;
    const auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        !at->second.owners.count(issuer))
        return false;
    auto& state = at->second;
    if (state.createIssuer == issuer && !state.createOwner) {
        state.createAssociation = 0;
        state.createExpires = 0;
    }
    if (state.worldIssuer == issuer && !state.worldOwner) {
        state.worldEntry = WorldEntry();
        state.worldExpires = 0;
        state.worldIssuer = 0;
    }
    return true;
}
void Server::Release(uint64_t owner) {
    const auto affected = [this, owner](const SessionState& state) {
        const auto pending = pendingAdmissions_.find(owner);
        return state.owners.count(owner) || state.createIssuer == owner || state.createOwner == owner ||
               state.worldOwner == owner || (pending != pendingAdmissions_.end() && pending->second == state.session);
    };
    for (;;) {
        std::set<std::string> logins;
        {
            StateGuard guard(*this);
            for (const auto& pair : sessions_)
                if (affected(pair.second))
                    logins.insert(pair.first);
        }
        // A disconnect may wait for its own account, but never with the shared
        // state locked. Multiple account locks always use the same sorted order.
        std::vector<std::unique_ptr<AccountOperation>> operations;
        for (const auto& login : logins)
            operations.emplace_back(new AccountOperation(*this, login));
        StateGuard guard(*this);
        bool retry = false;
        for (const auto& pair : sessions_)
            if (affected(pair.second) && !logins.count(pair.first))
                retry = true;
        if (retry)
            continue; // Release all locks, then include newly associated accounts.
        for (auto& pair : sessions_)
            if (pair.second.worldOwner == owner)
                WaitWorldTargetsLocked(pair.second.worldTargets);
        // Waiting released the shared state; include any newly associated account.
        for (const auto& pair : sessions_)
            if (affected(pair.second) && !logins.count(pair.first))
                retry = true;
        if (retry)
            continue;
        ownerSockets_.erase(owner);
        for (auto& pair : sessions_) {
            if (pair.second.createIssuer == owner)
                pair.second.createAssociation = 0;
            if (pair.second.createOwner == owner) {
                pair.second.createOwner = 0;
                pair.second.createSaved = false;
                pair.second.createdCharacter = Guid();
            }
            if (pair.second.worldOwner == owner) {
                LeavePeersLocked(pair.second);
                pair.second.outgoing.clear();
                pair.second.outgoingBytes = 0;
                pair.second.outgoingOverflow = false;
                pair.second.worldOwner = 0;
                pair.second.worldPrepared = false;
                pair.second.worldReady = false;
                pair.second.activeWorld.reset();
                pair.second.gameMessages.reset();
                pair.second.worldActions.reset();
            }
            if (pair.second.owners.erase(owner) && pair.second.owners.empty())
                pair.second.expires = clock_() + 300000;
        }
        Prune();
        return;
    }
}
} // namespace glide
