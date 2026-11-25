#include "server.h"
#include "glide/msgconn_stream.h"
#include <Windows.h>
#include <ctime>
#include <cstdio>
#include <cstring>

namespace glide {
namespace {
bool DeleteOwnedCharacter(Server& server, const std::shared_ptr<const LoginSession>& session, const std::string& name,
                          std::string& error) {
    AccountState account, saved;
    if (name.empty() || !session || !server.LoadState(session, account, error))
        return false;
    for (const auto& character : account.characters)
        if (character.name == name)
            return server.DeleteCharacter(session, character.guid, saved, error) == DeleteResult::Saved;
    error = "Character does not belong to this account";
    return false;
}
class AccountServices : public AuthServices {
  public:
    AccountServices(Server& server, uint64_t owner) : server_(server), owner_(owner) {}
    uint64_t ConnectionId() const override {
        return owner_;
    }
    bool Active() override {
        return session_ && server_.SessionActive(session_);
    }
    bool RevokePending() override {
        return server_.RevokePending(session_, owner_);
    }
    uint16_t Port() const override {
        return server_.Port();
    }
    bool Refer(uint32_t route, uint32_t requestedMap, uint32_t context, unsigned slots, uint32_t& association,
               uint32_t& map, std::string& error) override {
        if (!owner_ || !Active())
            return false;
        uint32_t issued = 0, issuedMap = 0;
        if (route == 1) {
            if (requestedMap || context || !server_.IssueBoundCreate(session_, owner_, slots, issued))
                return false;
        } else if (route == 3) {
            WorldEntry entry;
            if (!server_.IssueBoundWorld(session_, owner_, requestedMap, context, entry, error))
                return false;
            issued = entry.association;
            issuedMap = entry.character.map;
        } else
            return false;
        association = issued;
        map = issuedMap;
        return true;
    }
    bool Claim(ByteView identity) override {
        if (!identity.Valid() || identity.size != 32)
            return false;
        if (session_)
            return Active() && identity_.Size() == identity.size &&
                   std::memcmp(identity_.Data(), identity.data, identity.size) == 0;
        Buffer copy;
        if (!copy.Append(identity))
            return false;
        session_ = server_.ClaimAuthIdentity(owner_, identity);
        if (!session_)
            return false;
        identity_ = std::move(copy);
        return true;
    }
    bool Load(AccountState& out, std::string& error) override {
        return Active() && server_.LoadState(session_, out, error);
    }
    bool PrepareLogin(ByteView identity, AccountState& out, std::string& error) override {
        if (!owner_ || !identity.Valid() || identity.size != 32)
            return false;
        Buffer copy;
        if (!copy.Append(identity))
            return false;
        std::shared_ptr<const LoginSession> prepared;
        AccountState account;
        if (session_) {
            if (!Active() || identity_.Size() != identity.size ||
                std::memcmp(identity_.Data(), identity.data, identity.size) ||
                !server_.LoadState(session_, account, error))
                return false;
            prepared = session_;
        } else if (!server_.PrepareTokenAuth(identity, prepared, account, error))
            return false;
        prepared_ = std::move(prepared);
        preparedIdentity_ = std::move(copy);
        out = std::move(account);
        return true;
    }
    bool ClaimPrepared(ByteView identity) override {
        if (!prepared_ || !identity.Valid() || identity.size != 32 || preparedIdentity_.Size() != 32 ||
            std::memcmp(preparedIdentity_.Data(), identity.data, 32))
            return false;
        if (session_)
            return prepared_ == session_ && Active();
        auto admitted = server_.ClaimTokenAuthIdentity(owner_, identity, prepared_);
        if (!admitted)
            return false;
        session_ = std::move(admitted);
        identity_ = std::move(preparedIdentity_);
        prepared_.reset();
        return true;
    }
    bool Select(const std::string& name, AccountState& out, std::string& error) override {
        return Active() && server_.SelectCharacter(session_, name, out, error) == SelectResult::Selected;
    }
    bool Delete(const std::string& name, std::string& error) override {
        return Active() && DeleteOwnedCharacter(server_, session_, name, error);
    }
    bool SaveConfig(ByteView bytes, std::string& error) override {
        return Active() && server_.SaveAccountConfig(session_, bytes, error);
    }
    bool Agreement(uint8_t version, std::string& error) override {
        return Active() && server_.SaveAgreement(session_, version, error);
    }
    bool Assign(uint32_t route, uint32_t& association, uint32_t& map, uint32_t& nonce, std::string& error) override {
        if (!Active())
            return false;
        if (route == 1) {
            if (!server_.IssueCreate(session_, owner_, association))
                return false;
            map = 0;
        } else {
            WorldEntry entry;
            if (!server_.IssueWorld(session_, owner_, entry, error))
                return false;
            association = entry.association;
            map = entry.character.map;
        }
        nonce = 0xb0000000u | (association & 0x0fffffffu);
        return true;
    }

  private:
    Server& server_;
    uint64_t owner_;
    std::shared_ptr<const LoginSession> session_;
    Buffer identity_;
    std::shared_ptr<const LoginSession> prepared_;
    Buffer preparedIdentity_;
};
void RunProfileAuth(Server& server, Socket& socket, MsgConnStream& connection, const BuildProfile& profile,
                    uint64_t owner, AuthFlow& flow) {
    AccountServices services(server, owner);
    Buffer input;
    std::string error;
    while (!socket.Stopped()) {
        size_t consumed = 0;
        std::vector<Buffer> frames;
        const auto status = flow.Step(input.View(), services, consumed, frames, error);
        if (status == DecodeStatus::Incomplete) {
            // An authenticated player may spend minutes choosing a character.
            // Keep the connection (and its creation permits) while idle, like CREATE.
            // Partial messages and unauthenticated peers retain the read timeout.
            if (!input.Size() && services.Active() && !connection.HasBufferedInput() && !socket.WaitReadable(500))
                continue;
            uint8_t bytes[8192];
            size_t count = 0;
            if (!connection.Read(bytes, sizeof(bytes), count) || !count || input.Size() + count > 2 * 1024 * 1024 ||
                !input.Append(ByteView(bytes, count)))
                return;
            if (server.Verbose()) {
                Reader head(input.View());
                uint16_t id = 0;
                head.U16(id);
                server.Logger().Write("auth", "Received id=" + std::to_string(id) +
                                                  " buffered=" + std::to_string(input.Size()));
            }
            continue;
        }
        if (status != DecodeStatus::Complete || !consumed || consumed > input.Size()) {
            server.Logger().Write("auth", error.empty() ? "Profile rejected Auth request" : error);
            return;
        }
        for (const auto& frame : frames)
            if (!connection.Send(frame.View(), profile.Transport().serverFrames))
                return;
        if (server.Verbose())
            server.Logger().Write("auth", "Processed bytes=" + std::to_string(consumed) +
                                              " replies=" + std::to_string(frames.size()));
        input.Consume(consumed);
    }
}
} // namespace
void RunCreate(Server& server, Socket& socket, MsgConnStream& connection,
               const std::shared_ptr<const LoginSession>& session, uint64_t owner);
void RunGame(Server& server, Socket& socket, MsgConnStream& connection,
             const std::shared_ptr<const LoginSession>& session, uint64_t owner);
std::shared_ptr<const LoginSession> Server::ClaimGameInitializer(uint64_t owner, const GameInitializerCodec& codec,
                                                                 const InitHeader& header, bool& create,
                                                                 WorldEntry& world) {
    GameJoinRequest request;
    if (!codec.DecodeGame(header, request))
        return nullptr;
    if (Verbose())
        log_.Write("msgconn",
                   "Game request map=" + std::to_string(request.map) + " context=" + std::to_string(request.context));
    const ByteView user(request.user.bytes, 16), character(request.character.bytes, 16);
    if (!request.map) {
        const auto session = ClaimBoundCreate(owner, request.association, request.context, user, character);
        if (session)
            create = true;
        return session;
    }
    const auto session =
        ClaimBoundWorld(owner, request.association, request.map, request.context, user, character, world);
    if (session)
        create = false;
    return session;
}
void RunMsgConn(Server& server, Socket& socket, uint64_t owner, ByteView initial) {
    MsgConnStream connection(socket);
    InitHeader init;
    std::string error;
    if (server.Verbose())
        server.Logger().Write("msgconn", "Reading initializer; received=" + std::to_string(initial.size));
    if (!connection.ReadInit(initial, init)) {
        server.Logger().Write("msgconn", "Invalid or incomplete initializer");
        return;
    }
    if (server.Verbose())
        server.Logger().Write("msgconn", "Initializer build=" + std::to_string(init.build) +
                                             " program=" + std::to_string(init.program) +
                                             " extension=" + std::to_string(init.extension.size));
    const auto* candidate = BuildRegistry::Find(init.build);
    if (!candidate || init.program != candidate->Transport().program || !server.Bind(init.build, error)) {
        server.Logger().Write("msgconn", "Initializer does not match active profile: " + error);
        return;
    }
    const auto& profile = *candidate;
    const auto* initializers = profile.GameInitializers();
    if (!init.extension.size && initializers && !initializers->ValidAuth(init))
        return;
    if (init.extension.size) {
        if (initializers) {
            bool create = false;
            WorldEntry world;
            const auto session = server.ClaimGameInitializer(owner, *initializers, init, create, world);
            if (!session) {
                server.Logger().Write("msgconn", "Game initializer admission rejected");
                return;
            }
            if (server.Verbose())
                server.Logger().Write("msgconn", "Game cipher negotiation started");
            if (!connection.StartCipher(profile.Transport().keyRecord)) {
                server.Logger().Write("msgconn", "Game key exchange failed");
                return;
            }
            if (server.Verbose())
                server.Logger().Write("msgconn", "Game cipher negotiation completed");
            if (create)
                RunCreate(server, socket, connection, session, owner);
            else
                RunGame(server, socket, connection, session, owner);
            return;
        }
        Reader extra(init.extension);
        uint32_t association, ignored, map;
        if (!extra.U32(association) || !extra.U32(ignored) || !extra.U32(map))
            return;
        if (map) {
            uint32_t context;
            if (map > 65535 || !extra.U32(context))
                return;
            WorldEntry entry;
            const auto session =
                server.ClaimWorld(owner, association, map, context, ByteView(init.extension.data + 64, 4), entry);
            if (!session || !connection.StartCipher(profile.Transport().keyRecord))
                return;
            RunGame(server, socket, connection, session, owner);
            return;
        }
        const auto session = server.ClaimCreate(owner, association, ByteView(init.extension.data + 16, 16));
        if (!session || !connection.StartCipher(profile.Transport().keyRecord))
            return;
        RunCreate(server, socket, connection, session, owner);
        return;
    }
    if (server.Verbose())
        server.Logger().Write("auth", "Waiting for key exchange");
    if (!connection.StartCipher(profile.Transport().keyRecord)) {
        server.Logger().Write("auth", "Key exchange failed");
        return;
    }
    if (server.Verbose())
        server.Logger().Write("auth", "Key exchange complete; waiting for login message");
    std::unique_ptr<AuthFlow> flow;
    if (!profile.CreateAuthFlow(server.DataDirectory(), flow, error)) {
        server.Logger().Write("auth", error);
        return;
    }
    if (flow) {
        RunProfileAuth(server, socket, connection, profile, owner, *flow);
        return;
    }
    const auto& auth = server.Auth();
    Buffer input;
    std::shared_ptr<const LoginSession> session;
    for (;;) {
        AuthMessage message;
        const auto status = auth.Decode(input.View(), session != nullptr, message);
        if (status == DecodeStatus::Incomplete) {
            // Keep an authenticated character selection open while the player
            // is idle, as the profile Auth flow already does. Partial messages
            // and unauthenticated connections still use the read timeout.
            if (!input.Size() && session && !connection.HasBufferedInput() && !socket.WaitReadable(500))
                continue;
            uint8_t bytes[8192];
            size_t count = 0;
            if (!connection.Read(bytes, sizeof(bytes), count) || input.Size() + count > 2 * 1024 * 1024 ||
                !input.Append(ByteView(bytes, count)))
                return;
            continue;
        }
        if (status != DecodeStatus::Complete) {
            Reader head(input.View());
            uint16_t id = 0;
            head.U16(id);
            char diagnostic[144];
            std::snprintf(diagnostic, sizeof(diagnostic),
                          "%s message id=0x%04x buffered=%u authenticated=%u; connection closed",
                          status == DecodeStatus::Unknown ? "Unknown" : "Malformed", static_cast<unsigned>(id),
                          static_cast<unsigned>(input.Size()), session ? 1u : 0u);
            server.Logger().Write("auth", diagnostic);
            return;
        }
        if (message.action == AuthAction::Login) {
            session = server.ClaimAuthIdentity(owner, message.identity);
            if (!session) {
                server.Logger().Write("auth", "No unique pending login authorization");
                return;
            }
            AccountState account;
            if (!server.LoadState(session, account, error)) {
                server.Logger().Write("auth", error);
                return;
            }
            const auto now = std::time(nullptr);
            const uint32_t seconds = now > 946684800 ? static_cast<uint32_t>(now - 946684800) : 0;
            std::vector<Buffer> frames;
            if (!auth.Login(account, message.handshake, message.request, GetTickCount(), seconds, frames)) {
                server.Logger().Write("auth", "Cannot encode saved characters for this client");
                return;
            }
            for (const auto& frame : frames)
                if (!connection.Send(frame.View(), profile.Transport().serverFrames))
                    return;
            server.Logger().Write("auth", "Character list sent");
        } else if (message.action == AuthAction::SessionStatus) {
            Buffer reply;
            if (!auth.SessionStatus(owner, reply) || !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
        } else if (message.action == AuthAction::Reply) {
            Buffer reply;
            if (!session || !auth.Reply(message, reply) ||
                !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
        } else if (message.action == AuthAction::SelectCharacter) {
            if (!session)
                return;
            AccountState state;
            const auto selected = server.SelectCharacter(session, message.characterName, state, error);
            if (selected == SelectResult::Failed) {
                server.Logger().Write("auth", error);
                return;
            }
            Buffer reply;
            const auto now = std::time(nullptr);
            const uint32_t seconds = now > 946684800 ? static_cast<uint32_t>(now - 946684800) : 0;
            if (!auth.Selection(selected == SelectResult::Selected ? &state : nullptr, message.request, seconds,
                                reply) ||
                !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
        } else if (message.action == AuthAction::DeleteCharacter) {
            Buffer reply;
            if (!session || !auth.Deletion(message.request, reply))
                return;
            if (!DeleteOwnedCharacter(server, session, message.characterName, error)) {
                server.Logger().Write("auth", error);
                return;
            }
            if (!connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
        } else if (message.action == AuthAction::CharacterList) {
            AccountState account;
            std::vector<Buffer> frames;
            if (!session || !server.LoadState(session, account, error) ||
                !auth.CharacterList(account, message.request, frames))
                return;
            for (const auto& frame : frames)
                if (!connection.Send(frame.View(), profile.Transport().serverFrames))
                    return;
        } else if (message.action == AuthAction::Assignment) {
            if (!session)
                return;
            uint32_t association, map = 0;
            Buffer reply;
            if (message.route == 1) {
                if (!server.IssueCreate(session, owner, association))
                    return;
            } else {
                WorldEntry entry;
                if (!server.IssueWorld(session, owner, entry, error)) {
                    server.Logger().Write("auth", error);
                    return;
                }
                association = entry.association;
                map = entry.character.map;
            }
            if (!auth.Assignment(message, association, map, 0xb0000000u | (association & 0x0fffffffu), reply) ||
                !connection.Send(reply.View(), profile.Transport().serverFrames))
                return;
        } else if (message.action != AuthAction::Control) {
            server.Logger().Write("auth", "Message handler pending: " + std::to_string(message.id));
            return;
        }
        input.Consume(message.consumed);
    }
}
} // namespace glide
