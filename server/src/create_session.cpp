#include "server.h"
#include "glide/msgconn_stream.h"
#include "glide/crypto.h"
#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace glide {
bool Server::IssueCreate(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, uint32_t& association) {
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    return IssueCreateLocked(session, issuer, association, false, 0);
}
bool Server::IssueBoundCreate(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, unsigned slots,
                              uint32_t& association) {
    if (!issuer || !slots || slots > 5)
        return false;
    AccountOperation accountOperation(*this, session ? session->login : std::string());
    StateGuard guard(*this);
    Prune();
    return IssueCreateLocked(session, issuer, association, true, slots);
}
bool Server::IssueCreateLocked(const std::shared_ptr<const LoginSession>& session, uint64_t issuer,
                               uint32_t& association, bool bound, unsigned slots) {
    if (!session)
        return false;
    auto at = sessions_.find(session->login);
    if (at == sessions_.end() || at->second.session != session || at->second.loggedOut ||
        !at->second.owners.count(issuer) || at->second.createOwner || at->second.worldOwner)
        return false;
    Guid user, selected;
    if (bound) {
        KvDocument document;
        AccountState account;
        std::string error;
        if (!accounts_ || !LoadAccountLocked(session->login, document, error) ||
            !DecodeAccountState(document, account, error) || account.characters.size() >= slots ||
            !Guid::Parse(session->userId, user) || std::memcmp(user.bytes, account.userGuid.bytes, 16))
            return false;
        bool found = account.characters.empty();
        for (const auto& character : account.characters)
            if (character.slot == account.selected) {
                selected = character.guid;
                found = true;
            }
        if (!found)
            return false;
    }
    uint32_t number;
    if (!RandomBytes(reinterpret_cast<uint8_t*>(&number), sizeof(number)))
        return false;
    number = 0xa0000000u | (number & 0x0fffffffu);
    for (const auto& pair : sessions_)
        if (pair.second.createAssociation == number || pair.second.worldEntry.association == number)
            return false;
    at->second.worldEntry.association = 0;
    at->second.createAssociation = number;
    at->second.createExpires = clock_() + 60000;
    at->second.createIssuer = issuer;
    at->second.createBound = bound;
    at->second.createSlots = slots;
    at->second.createUser = user;
    at->second.createSelected = selected;
    association = number;
    return true;
}
std::shared_ptr<const LoginSession> Server::ClaimCreate(uint64_t owner, uint32_t association, ByteView accountGuid) {
    if (!owner || !association || accountGuid.size != 16 || !accountGuid.Valid())
        return nullptr;
    std::shared_ptr<const LoginSession> candidate;
    {
        StateGuard guard(*this);
        Prune();
        candidate = FindClaimSessionLocked(association, true, false);
    }
    if (!candidate)
        return nullptr;
    AccountOperation accountOperation(*this, candidate->login);
    StateGuard guard(*this);
    Prune();
    return ClaimCreateLocked(owner, association, accountGuid, false, {}, candidate);
}
std::shared_ptr<const LoginSession> Server::ClaimBoundCreate(uint64_t owner, uint32_t association, uint32_t context,
                                                             ByteView user, ByteView character) {
    if (!owner || !association || context || !user.Valid() || user.size != 16 || !character.Valid() ||
        character.size != 16)
        return nullptr;
    std::shared_ptr<const LoginSession> candidate;
    {
        StateGuard guard(*this);
        Prune();
        candidate = FindClaimSessionLocked(association, true, true);
    }
    if (!candidate)
        return nullptr;
    AccountOperation accountOperation(*this, candidate->login);
    StateGuard guard(*this);
    Prune();
    return ClaimCreateLocked(owner, association, user, true, character, candidate);
}
std::shared_ptr<const LoginSession> Server::ClaimCreateLocked(uint64_t owner, uint32_t association,
                                                              ByteView accountGuid, bool bound, ByteView selectedGuid,
                                                              const std::shared_ptr<const LoginSession>& expected) {
    if (bound)
        for (const auto& pair : sessions_)
            if (pair.second.owners.count(owner))
                return nullptr;
    AdmissionReservation reservation(*this, owner, expected);
    if (!reservation.Reserved())
        return nullptr;
    for (auto& pair : sessions_) {
        auto& state = pair.second;
        if (state.session != expected || state.loggedOut || state.createBound != bound ||
            state.createAssociation != association || state.createExpires <= clock_() ||
            !state.owners.count(state.createIssuer) || state.createOwner)
            continue;
        Guid guid;
        if (!Guid::Parse(state.session->userId, guid) || !guid.Matches(accountGuid))
            return nullptr;
        if (bound) {
            if (std::memcmp(state.createUser.bytes, accountGuid.data, 16) ||
                std::memcmp(state.createSelected.bytes, selectedGuid.data, 16))
                return nullptr;
            KvDocument document;
            AccountState account;
            std::string error;
            if (!accounts_ || !LoadAccountLocked(state.session->login, document, error) ||
                !DecodeAccountState(document, account, error) || account.characters.size() >= state.createSlots ||
                std::memcmp(account.userGuid.bytes, accountGuid.data, 16))
                return nullptr;
            Guid selected;
            bool found = account.characters.empty();
            for (const auto& character : account.characters)
                if (character.slot == account.selected) {
                    selected = character.guid;
                    found = true;
                }
            if (!found || std::memcmp(selected.bytes, selectedGuid.data, 16))
                return nullptr;
        }
        if (state.createExpires <= clock_())
            return nullptr;
        if (bound)
            for (const auto& existing : sessions_)
                if (existing.second.owners.count(owner))
                    return nullptr;
        state.createAssociation = 0;
        state.createOwner = owner;
        state.createSaved = false;
        state.createdCharacter = Guid();
        state.createConfirmed = Guid();
        state.owners.insert(owner);
        return state.session;
    }
    return nullptr;
}
void RunCreate(Server& server, Socket& socket, MsgConnStream& connection,
               const std::shared_ptr<const LoginSession>& session, uint64_t owner) {
    const auto& profile = *server.Profile();
    const auto& flow = server.Creation();
    Buffer input;
    bool started = false, submitted = false, redirected = false;
    CreateHandshake handshake;
    Guid savedCharacter;
    uint64_t nextSignal = GetTickCount64() + 10000;
    while (!socket.Stopped()) {
        CreateMessage message;
        const auto status = flow.Decode(input.View(), message);
        if (status == DecodeStatus::Incomplete) {
            if (connection.HasBufferedInput() || socket.WaitReadable(500)) {
                uint8_t bytes[8192];
                size_t count = 0;
                if (!connection.Read(bytes, sizeof(bytes), count) || input.Size() + count > 2 * 1024 * 1024 ||
                    !input.Append(ByteView(bytes, count)))
                    return;
            }
            if ((!flow.RequiresReadyAcknowledgment() || started) && GetTickCount64() >= nextSignal) {
                Buffer signal;
                if (!flow.Signal(signal) || !connection.Send(signal.View(), profile.Transport().serverFrames))
                    return;
                nextSignal = GetTickCount64() + 10000;
            }
            continue;
        }
        if (status != DecodeStatus::Complete) {
            Reader head(input.View());
            uint16_t id = 0;
            head.U16(id);
            char diagnostic[128];
            std::snprintf(diagnostic, sizeof(diagnostic), "%s message id=0x%04x buffered=%u",
                          status == DecodeStatus::Unknown ? "Unknown" : "Malformed", static_cast<unsigned>(id),
                          static_cast<unsigned>(input.Size()));
            server.Logger().Write("create", diagnostic);
            return;
        }
        if (flow.RequiresReadyAcknowledgment() && !handshake.Accept(message.action))
            return;
        if (message.action == CreateAction::Close)
            return;
        if (message.action == CreateAction::Start && !started) {
            Buffer reply;
            if (!flow.Ready(reply) || !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
            started = true;
        } else if (message.action == CreateAction::Submit) {
            if (!started || submitted)
                return;
            const auto& point = flow.InitialLocation();
            message.character.map = point.map;
            message.character.x = point.x;
            message.character.y = point.y;
            message.character.z = point.z;
            Character created;
            std::string error;
            if (server.CreateCharacter(session, std::move(message.character), created, error, owner) !=
                CreateResult::Saved) {
                server.Logger().Write("create", error);
                return;
            }
            Guid account;
            Buffer reply;
            if (!Guid::Parse(session->userId, account) || !flow.Created(account, created, reply) ||
                !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
            submitted = true;
            savedCharacter = created.guid;
            server.Logger().Write("create", "Character saved");
        } else if (message.action == CreateAction::Complete) {
            if (!submitted) {
                server.Logger().Write("create", "Completion without a saved character");
                return;
            }
            if (!redirected) {
                Buffer redirect;
                std::string error;
                if (!server.CompleteCreate(session, owner, savedCharacter, redirect, error)) {
                    server.Logger().Write("create", error);
                    return;
                }
                if (!connection.Send(redirect.View(), profile.Transport().serverFrames))
                    return;
                redirected = true;
                server.Logger().Write("create", "World redirect sent");
            }
        }
        // Preview and controls never allocate a persistent character.
        input.Consume(message.consumed);
    }
}
} // namespace glide
