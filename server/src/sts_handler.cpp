#include "server.h"
#include "glide/sts.h"
#include "glide/tls.h"
#include "glide/xml.h"

namespace glide {
namespace {
bool EmptyReply(const StsVariant& variant, const std::string& path) {
    for (size_t i = 0; i < variant.emptyReplyCount; ++i)
        if (path == variant.emptyReplies[i])
            return true;
    return false;
}
bool Reply(Server& server, ByteStream& stream, const StsRequest& request, unsigned status, const char* statusText,
           const std::string& body) {
    if (request.sequence.empty())
        return true;
    server.Logger().Write("sts", request.path + " response=" + std::to_string(status) +
                                     " bodyBytes=" + std::to_string(body.size()));
    Buffer response;
    return EncodeStsResponseHeader(request.sequence, status, statusText, body.size(), response) &&
           response.Append(ByteView(body.data(), body.size())) && stream.Write(response.View());
}
bool Reject(Server& server, ByteStream& stream, const StsRequest& request, unsigned status, const char* statusText) {
    // The native STS reader defaults an absent code to zero (success), even for
    // a 4xx response. Code 4 is its generic error; do not invent service codes.
    return Reply(server, stream, request, status, statusText,
                 "<Error code=\"4\" text=\"" + XmlEscape(statusText) + "\"/>\n");
}
} // namespace
void RunSts(Server& server, ByteStream& socket, uint64_t owner, ByteView initial, TlsEntropy entropy) {
    Buffer input;
    if (!input.Append(initial))
        return;
    std::unique_ptr<TlsSession> tls;
    ByteStream* stream = &socket;
    bool connected = false;
    std::shared_ptr<const LoginSession> session;
    std::shared_ptr<const LoginSession> pendingLogin;
    for (;;) {
        StsRequest request;
        const auto status = DecodeStsRequest(input.View(), request);
        if (status == DecodeStatus::Incomplete) {
            uint8_t bytes[8192];
            size_t received = 0;
            if (!stream->Read(bytes, sizeof(bytes), received) ||
                input.Size() + received > kStsMaxBody + kStsMaxHeader + sizeof(bytes) ||
                !input.Append(ByteView(bytes, received)))
                return;
            continue;
        }
        if (status != DecodeStatus::Complete) {
            server.Logger().Write("sts", "Malformed request; connection closed");
            return;
        }
        server.Logger().Write("sts", request.path);
        if (session && !server.SessionActive(session))
            session.reset();
        bool ok = true;
        if (request.path == "/Sts/Connect") {
            std::string value, error;
            unsigned build = 0;
            if (!XmlValue(request.body, "Build", 10, value) || !ParseUnsigned(value, UINT32_MAX, build) ||
                !server.BindSts(build, error)) {
                server.Logger().Write("sts", "Rejected Connect build=" + std::to_string(build) +
                                                 (error.empty() ? ": missing or invalid Build field" : ": " + error));
                Reject(server, *stream, request, 400, "Bad Request");
                return;
            }
            connected = true;
        } else if (!connected) {
            Reject(server, *stream, request, 400, "Bad Request");
            return;
        } else if (request.path == "/Sts/Ping") {
            // These control messages have no response in the native protocol.
        } else if (request.path == "/Auth/StartTls") {
            if (tls || request.sequence.empty() || session)
                return;
            if (!Reply(server, *stream, request, 400, "Success",
                       "<Error server=\"1001\" module=\"4\" line=\"262\"/>\n"))
                return;
            input.Consume(request.consumed);
            std::string error;
            const bool password = server.Profile()->Sts().loginMode == StsLoginMode::TlsPassword;
            tls.reset(new TlsSession(socket, password ? TlsVariant::June2012 : TlsVariant::ResumedSession, entropy));
            if (password)
                pendingLogin = server.LoginSrp(owner, *tls, input.View(), error, entropy);
            if (password ? !pendingLogin : !tls->Accept(input.View(), error)) {
                server.Logger().Write("tls", error);
                return;
            }
            input.Clear();
            stream = tls.get();
            continue;
        } else if (request.path == "/Auth/GetHostname") {
            ok = Reply(server, *stream, request, 200, "OK",
                       std::string("<Reply><Hostname>") + server.Profile()->Sts().hostname +
                           "</Hostname><Local>true</Local></Reply>\n");
        } else if ((server.Profile()->Sts().tlsBeforeLogin ||
                    server.Profile()->Sts().loginMode == StsLoginMode::TlsPassword) &&
                   (!tls || !tls->Active())) {
            ok = Reject(server, *stream, request, 403, "Forbidden");
        } else if (request.path == "/Auth/LoginFinish") {
            if (server.Profile()->Sts().loginMode != StsLoginMode::TlsPassword || !tls || !tls->PasswordVerified() ||
                !pendingLogin || !server.SessionActive(pendingLogin)) {
                ok = Reject(server, *stream, request, 403, "Forbidden");
            } else {
                session = std::move(pendingLogin);
                ok = Reply(server, *stream, request, 200, "OK",
                           "<Reply><UserId>" + session->userId + "</UserId><UserName>" +
                               XmlEscape(session->displayName) + "</UserName><ResumeToken>" + session->resumeToken +
                               "</ResumeToken></Reply>\n");
            }
        } else if (server.Profile()->Sts().loginMode == StsLoginMode::TlsPassword &&
                   (request.path == "/Auth/StartSsoLogin" || request.path == "/Auth/Resume")) {
            ok = Reject(server, *stream, request, 403, "Forbidden");
        } else if (request.path == "/Auth/StartSsoLogin" || request.path == "/Auth/Resume") {
            std::string login, password, token;
            const bool resume = request.path == "/Auth/Resume";
            if ((!resume && (!XmlValue(request.body, "LoginName", 190, login) ||
                             !XmlValue(request.body, "Password", 5464, password) ||
                             (server.Profile()->Sts().passwordBase64 && !DecodeStsPassword(password, password)))) ||
                (resume && !XmlValue(request.body, "ResumeToken", 36, token))) {
                ok = Reject(server, *stream, request, 400, "Bad Request");
            } else if (session && (!resume || session->resumeToken != token)) {
                ok = Reject(server, *stream, request, 403, "Forbidden");
            } else {
                if (!session)
                    session = resume ? server.Resume(owner, token) : server.Login(owner, login, password);
                if (!session)
                    ok = Reject(server, *stream, request, 403, "Forbidden");
                else {
                    const std::string extra = resume
                                                  ? "<UserCenter>5</UserCenter><UserName>" +
                                                        XmlEscape(session->displayName) + "</UserName><Parts></Parts>"
                                                  : "";
                    ok = Reply(server, *stream, request, 200, "OK",
                               "<Reply><UserId>" + session->userId + "</UserId>" + extra + "<ResumeToken>" +
                                   session->resumeToken + "</ResumeToken><EmailVerified>1</EmailVerified></Reply>\n");
                }
            }
        } else if (!session) {
            ok = Reject(server, *stream, request, 403, "Forbidden");
        } else if (request.path == "/Auth/LogoutMyClient") {
            if (!server.Logout(owner, session))
                return;
            session.reset();
            ok = Reply(server, *stream, request, 200, "OK", "<Reply></Reply>\n");
        } else if (request.path == "/Account/ListMyGameAccounts" || request.path == "/Auth/ListMyGameAccounts") {
            const auto* fixedAlias = server.Profile()->Sts().gameAccountAlias;
            const std::string alias = fixedAlias ? fixedAlias : "Guild Wars 2 - " + session->displayName;
            ok = Reply(server, *stream, request, 200, "OK",
                       "<Reply type=\"array\">\n<Row>\n<Alias>" + XmlEscape(alias) + "</Alias>\n</Row>\n</Reply>\n");
        } else if (request.path == "/Auth/RequestGameToken") {
            std::string token;
            if (!server.IssueGameToken(session, token))
                return;
            ok = Reply(server, *stream, request, 200, "OK", "<Reply><Token>" + token + "</Token></Reply>\n");
        } else if (request.path == "/Presence/SetWatch") {
            const std::string body =
                "<Message><Status>online</Status><Aliases></Aliases><OnlineTimes></OnlineTimes>"
                "<AppData></AppData><Channels></Channels><Groups></Groups><Contacts></Contacts><UserId>" +
                session->userId + "</UserId><UserName>" + XmlEscape(session->displayName) +
                "</UserName><NewBeginning></NewBeginning></Message>\n";
            const std::string message = "l:" + std::to_string(body.size()) + "\r\n\r\n" + body;
            ok = Reply(server, *stream, request, 200, "OK", "<Reply></Reply>\n") &&
                 stream->Write(ByteView(message.data(), message.size()));
        } else if (EmptyReply(server.Profile()->Sts(), request.path)) {
            ok = Reply(server, *stream, request, 200, "OK", "<Reply></Reply>\n");
        } else {
            ok = Reject(server, *stream, request, 404, "Not Found");
        }
        if (!ok)
            return;
        input.Consume(request.consumed);
    }
}
} // namespace glide
