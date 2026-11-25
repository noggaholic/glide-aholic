#include "auth_flow.h"
#include "auth_request.h"
#include "auth.h"
#include "glide/profile/auth_codec.h"
#include "glide/files.h"
#include <algorithm>

namespace glide {
const AuthCodec& Build204489Auth();
namespace china167409 {
namespace {
bool Zeroes(Buffer& out, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!out.U8(0))
            return false;
    return true;
}
bool Ack(uint32_t request, Buffer& out, uint32_t a = 0, uint32_t b = 0, uint32_t c = 0) {
    return out.U16(4) && out.VarU32(request) && out.VarU32(0) && out.VarU32(a) && out.VarU32(b) && out.VarU32(c);
}
bool Roster(const AccountState& account, uint32_t request, Buffer& out, const CharacterCodec* characters = nullptr) {
    for (const auto& character : account.characters) {
        Buffer summary;
        if (!(characters ? characters->Summary(character, summary) : Build204489Auth().Summary(character, summary)) ||
            summary.Size() > 512 || !out.U16(0x0011) || !out.VarU32(request) ||
            !out.Append(ByteView(character.guid.bytes, 16)) || !out.VarU32(0) || !out.U64(0) || !out.U64(0) ||
            !out.Utf16Z(character.name.c_str(), 19) || !out.U16(static_cast<uint16_t>(summary.Size())) ||
            !out.Append(summary.View()))
            return false;
    }
    return true;
}
bool Account(const AccountState& account, uint32_t request, uint64_t unixSeconds, Buffer& out) {
    if (unixSeconds < 946684800ULL || unixSeconds - 946684800ULL > 0xffffffffULL ||
        std::all_of(account.userGuid.bytes, account.userGuid.bytes + 16, [](uint8_t b) { return b == 0; }))
        return false;
    unsigned revision = 0;
    const auto count = account.document.Count("", "agreement_version");
    if (count) {
        std::string value;
        if (count != 1 || !account.document.Get("", "agreement_version", value) || !ParseUnsigned(value, 255, revision))
            return false;
    }
    const Guid* selected = nullptr;
    for (const auto& character : account.characters)
        if (character.slot == account.selected) {
            if (selected)
                return false;
            selected = &character.guid;
        }
    if (!account.characters.empty() && !selected)
        return false;
    const uint8_t zero[16] = {};
    if (!out.U16(8) || !out.VarU32(request) || !out.Append(ByteView(account.userGuid.bytes, 16)) ||
        !out.Append(ByteView(selected ? selected->bytes : zero, 16)))
        return false;
    const uint32_t values[] = {account.accessLevel,
                               revision,
                               static_cast<uint32_t>(unixSeconds - 946684800ULL),
                               account.homeWorld,
                               account.homeWorld,
                               account.worldClass,
                               0};
    for (auto value : values)
        if (!out.VarU32(value))
            return false;
    // China has four base slots. Entitlement 1 adds the fifth; its U16 count
    // belongs to the account's native entitlement array, not Auth 0x1b.
    return out.U8(0) && Zeroes(out, 32) && out.U8(2) && out.U16(45) && out.U16(1) && out.VarU32(0) && out.VarU32(0) &&
           out.VarU32(0) && out.U16(1) && out.U16(1) && out.VarU32(0) && out.VarU32(0) && out.VarU32(0) &&
           Zeroes(out, 32);
}
bool Name(Reader& reader, std::string& name) {
    std::wstring wide;
    uint16_t word;
    while (reader.U16(word)) {
        if (!word)
            return WideToUtf8(wide, name);
        wide.push_back(static_cast<wchar_t>(word));
    }
    return false;
}
class ChinaAuthFlow final : public glide::AuthFlow {
  public:
    ChinaAuthFlow(std::function<uint64_t()> seconds, std::function<uint32_t()> ticks,
                  std::unique_ptr<CharacterCodec> characters)
        : seconds_(std::move(seconds)), ticks_(std::move(ticks)), characters_(std::move(characters)) {}
    DecodeStatus Step(ByteView input, AuthServices& services, size_t& consumed, std::vector<Buffer>& frames,
                      std::string& error) override {
        AuthRequest request;
        const auto status = DecodeAuthRequest(input, request);
        if (status != DecodeStatus::Complete)
            return status;
        std::vector<Buffer> result;
        if (!Handle(request, services, result, error)) {
            if (error.empty())
                error = "China Auth request is not valid in the current login state";
            return DecodeStatus::Malformed;
        }
        consumed = request.consumed;
        frames = std::move(result);
        error.clear();
        return DecodeStatus::Complete;
    }

  private:
    enum class Phase { Login, Content, Ready };
    Phase phase_ = Phase::Login;
    uint32_t loginRequest_ = 0;
    std::function<uint64_t()> seconds_;
    std::function<uint32_t()> ticks_;
    std::unique_ptr<CharacterCodec> characters_;
    bool CompleteLogin(const AccountState& account, std::vector<Buffer>& frames) {
        Buffer roster, post, ack;
        if (!Roster(account, loginRequest_, roster, characters_.get()) || !roster.U16(0x000a) ||
            !roster.VarU32(loginRequest_) || !roster.U16(0))
            return false;
        // Original Auth 0x1b: U8 count of {Var feature, Var value}, not a U16 ID-only list.
        if (!post.U16(0x001b) || !post.VarU32(loginRequest_) || !post.U8(1) || !post.VarU32(45) || !post.VarU32(1) ||
            !post.U16(0x001d) || !post.VarU32(account.instanceType) || !post.U16(0x0010) ||
            !post.VarU32(loginRequest_) || !post.Append(ByteView(account.userGuid.bytes, 16)) || !post.VarU32(0) ||
            !post.VarU32(ticks_()) || !post.VarU32(0) || !post.VarU32(0) || !post.VarU32(0) || !post.U16(0x0017) ||
            !post.U16(0x0018) || !Ack(loginRequest_, ack))
            return false;
        frames.push_back(std::move(roster));
        frames.push_back(std::move(post));
        frames.push_back(std::move(ack));
        return true;
    }
    bool Handle(const AuthRequest& request, AuthServices& services, std::vector<Buffer>& frames, std::string& error) {
        Reader reader(request.payload);
        uint32_t id = 0, value = 0;
        Buffer reply;
        switch (request.id) {
        case 0x0001:
        case 0x0002:
        case 0x0005:
        case 0x000b:
        case 0x001a:
        case 0x0022:
        case 0x0023:
        case 0x001e:
            return true;
        case 0x0003:
            if (!reply.U16(2) || !reply.VarU32(static_cast<uint32_t>(services.ConnectionId())))
                return false;
            break;
        case 0x000a: {
            ByteView identity;
            if (phase_ != Phase::Login || reader.VarU32(id) != DecodeStatus::Complete || !reader.Bytes(32, identity))
                return false;
            AccountState account;
            if (!services.PrepareLogin(identity, account, error))
                return false;
            Buffer validatedRoster;
            if (!Account(account, id, seconds_(), reply) || !Roster(account, id, validatedRoster, characters_.get()) ||
                !services.ClaimPrepared(identity))
                return false;
            loginRequest_ = id;
            phase_ = Phase::Content;
            break;
        }
        case 0x0004: {
            if (phase_ != Phase::Content || !services.Active())
                return false;
            AccountState account;
            if (!services.Load(account, error) || !CompleteLogin(account, frames))
                return false;
            phase_ = Phase::Ready;
            return true;
        }
        case 0x000c: {
            uint16_t length;
            ByteView config;
            if (phase_ == Phase::Login || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete ||
                !reader.U16(length) || !reader.Bytes(length, config) || !services.SaveConfig(config, error) ||
                !Ack(id, reply))
                return false;
            break;
        }
        case 0x000e:
            if (phase_ == Phase::Login || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete ||
                reader.VarU32(value) != DecodeStatus::Complete || !Ack(id, reply))
                return false;
            break;
        case 0x000f: {
            uint8_t revision;
            return phase_ != Phase::Login && services.Active() && reader.U8(revision) && revision == 5 &&
                   services.Agreement(revision, error);
        }
        case 0x0014: {
            if (phase_ != Phase::Ready || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete)
                return false;
            std::string name;
            if (!Name(reader, name) || !services.Delete(name, error) || !Ack(id, reply))
                return false;
            break;
        }
        case 0x0016: {
            if (phase_ != Phase::Ready || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete)
                return false;
            std::string name;
            if (!Name(reader, name))
                return false;
            AccountState account;
            if (!services.Select(name, account, error)) {
                error.clear();
                if (!Ack(id, reply))
                    return false;
            } else if (!Account(account, id, seconds_(), reply) || !Ack(id, reply, 1002, 3, 3516))
                return false;
            break;
        }
        case 0x0019: {
            if (phase_ != Phase::Ready || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete)
                return false;
            AccountState account;
            Buffer roster;
            if (!services.Load(account, error) || !reply.U16(0x0017) || !reply.U16(0x0018) ||
                !Roster(account, id, roster, characters_.get()) || !Ack(id, roster))
                return false;
            frames.push_back(std::move(reply));
            frames.push_back(std::move(roster));
            return true;
        }
        case 0x0027:
            // Reuse the existing build 204489 transaction failure for this unsupported
            // account request. Its complete China schema is decoded before here.
            if (phase_ != Phase::Ready || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete || !id ||
                !Ack(id, reply, 4, 5, 5121))
                return false;
            break;
        case 0x001d: {
            uint32_t route = 0, ignored = 0;
            if (phase_ != Phase::Ready || !services.Active() || reader.VarU32(id) != DecodeStatus::Complete ||
                reader.VarU32(route) != DecodeStatus::Complete || (route != 1 && route != 3))
                return false;
            for (unsigned i = 0; i < 4; ++i)
                if (reader.VarU32(ignored) != DecodeStatus::Complete)
                    return false;
            uint32_t association = 0, map = 0, nonce = 0;
            if (!services.Assign(route, association, map, nonce, error) || !services.Port())
                return false;
            uint8_t endpoint[28] = {2, 0, 0, 0, 127, 0, 0, 1};
            endpoint[2] = static_cast<uint8_t>(services.Port() >> 8);
            endpoint[3] = static_cast<uint8_t>(services.Port());
            if (!reply.U16(0x0014) || !reply.VarU32(id) || !reply.VarU32(route) || !reply.VarU32(association) ||
                !reply.VarU32(map) || !reply.Append(ByteView(endpoint, sizeof(endpoint))) || !reply.VarU32(nonce))
                return false;
            break;
        }
        default:
            return false;
        }
        frames.push_back(std::move(reply));
        return true;
    }
};
} // namespace
std::unique_ptr<glide::AuthFlow> CreateAuthFlow(std::function<uint64_t()> seconds, std::function<uint32_t()> ticks,
                                                std::unique_ptr<CharacterCodec> characters) {
    if (!seconds || !ticks || !characters)
        return {};
    return std::unique_ptr<glide::AuthFlow>(
        new ChinaAuthFlow(std::move(seconds), std::move(ticks), std::move(characters)));
}
bool EncodeAccount(const AccountState& account, uint32_t request, uint64_t seconds, Buffer& out) {
    Buffer result;
    if (!Account(account, request, seconds, result))
        return false;
    out = std::move(result);
    return true;
}
bool EncodeRoster(const AccountState& account, uint32_t request, Buffer& out) {
    Buffer result;
    if (!Roster(account, request, result))
        return false;
    out = std::move(result);
    return true;
}
} // namespace china167409
} // namespace glide
