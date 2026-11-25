#include "bootstrap.h"
#include "health_state.h"
#include "glide/profile/auth_codec.h"
#include "glide/profile/game_messages.h"
#include "glide/profile/modern_legends.h"
#include <cmath>
#include <initializer_list>
#include <limits>

namespace glide {
const AuthCodec& Build204489Auth();
const GameMessages& Build204489Game();
namespace {
constexpr uint16_t kCharacter = 57344;

bool Scalars(Buffer& out, std::initializer_list<uint32_t> values) {
    for (auto value : values)
        if (!out.VarU32(value))
            return false;
    return true;
}
bool Floats(Buffer& out, std::initializer_list<float> values) {
    for (auto value : values)
        if (!out.F32(value))
            return false;
    return true;
}
bool Zeros(Buffer& out, size_t count) {
    for (size_t i = 0; i < count; ++i)
        if (!out.U8(0))
            return false;
    return true;
}
class Recipe {
  public:
    Recipe(const BootstrapContext& context, uint16_t glider, const ModernWorldDialect& dialect,
           const PeerIdentity* peer = nullptr)
        : context_(context), character_(context.entry.character), dialect_(dialect), glider_(glider),
          remote_(peer != nullptr), kCharacter(peer ? peer->character : 57344), kPlayer(peer ? peer->player : 1),
          kFirstBag(peer ? peer->firstBag : 20) {}
    template <class Fields> bool Message(const char* name, uint16_t id, Fields fields) {
        BootstrapStep step;
        step.name = name;
        if (!step.payload.U16(dialect_.Id(id)) || !fields(step.payload) || step.payload.Size() > 60000)
            return false;
        steps.push_back(std::move(step));
        return true;
    }
    bool Message(const char* name, uint16_t id, std::initializer_list<uint32_t> scalars = {}) {
        return Message(name, id, [&](Buffer& b) { return Scalars(b, scalars); });
    }
    void Wait(const char* name, GameConfirmation confirmation, uint32_t timeout) {
        BootstrapStep step;
        step.kind = BootstrapStep::Kind::WaitFor;
        step.name = name;
        step.confirmation = confirmation;
        step.timeoutMs = timeout;
        steps.push_back(std::move(step));
    }
    bool Collision(Buffer& b) const {
        return Floats(b, {character_.x * 32, character_.y * 32, character_.z == 0 ? 0 : -character_.z * 32});
    }
    bool MapIdentity() {
        const auto& map = context_.catalog.maps.at(character_.map);
        uint32_t data = character_.map, instance = 0x40000001u;
        if (map.type >= 9 && map.type <= 15)
            instance = 0xa0000001u;
        else if (map.type == 4)
            instance = context_.entry.account.homeWorld;
        else
            data |= 0x40000000u;
        return Message("mission character name", 0x0410,
                       [&](Buffer& b) { return b.Utf16Z(character_.name.c_str(), 20); }) &&
               Message("map data", 0x0428,
                       [&](Buffer& b) { return Scalars(b, {kPlayer, data, instance, 0}) && Zeros(b, 2); });
    }
    bool Time() {
        BootstrapStep step;
        step.name = "time of day";
        if (!Build204489Game().TimeOfDay(context_.timeTicks, context_.timeRate, context_.timePaused, step.payload) ||
            !RegionalHeader(step.payload))
            return false;
        steps.push_back(std::move(step));
        return true;
    }
    bool LoadPosition() {
        return Message("map load position", 0x042b, [&](Buffer& b) { return Collision(b); });
    }
    bool LoadMap() {
        return Message("load map", 0x0425, [&](Buffer& b) {
            return Collision(b) && b.U64(0) && b.U64(0) && b.U16(static_cast<uint16_t>(character_.map)) && b.U8(0) &&
                   b.U64(0) && b.VarU32(1);
        });
    }
    bool Agent() {
        return Message("owned character agent", 0x0025, [&](Buffer& b) {
            return b.U16(kCharacter) && b.VarU32(context_.sequence) && b.U8(0x80) && b.VarU32(0x00300000) &&
                   Zeros(b, 2) && b.VarU32(0x30000000u | kPlayer) && b.U8(remote_ ? 0 : 0x10) && b.U8(0x81) &&
                   b.U8(0) && Collision(b) && b.VarU32(0) && b.U16(0) && b.U16(0) &&
                   Floats(b, {294, 2.09439516f, 1, 70, 24, 0, 0, 0, 24});
        });
    }
    bool Player() {
        return (remote_ || Message("owned client control", 0x0053,
                                   [&](Buffer& b) { return b.U16(kCharacter) && b.VarU32(context_.sequence); })) &&
               Message("owned player", 0x01ab,
                       [&](Buffer& b) {
                           return b.VarU32(kPlayer) && b.Utf16Z(character_.name.c_str(), 32) &&
                                  b.Append(ByteView(character_.guid.bytes, 16)) && b.VarU32(0);
                       }) &&
               Agent() && Message("attach owned character", 0x01ba, {kPlayer, kCharacter}) &&
               // Build 204489 rank zero leaves the native rank-title definition unset. The original minimum is one.
               (dialect_.build != 204489 || Message("player initial WvW rank", 0x02c5, [&](Buffer& b) {
                    return Scalars(b, {kPlayer, 0, 0}) && b.U16(1);
                }));
    }
    bool Core(ByteView appearance, uint8_t profession, uint8_t effectiveLevel) {
        static const uint16_t attributes[][3] = {
            {0, 0, 0}, {1090, 1000, 90}, {1810, 1000, 810}, {1810, 1000, 810}, {1190, 1000, 190}, {0, 0, 0},
            {0, 0, 0}, {1588, 0, 1588},  {0, 0, 0},         {0, 0, 0},         {0, 0, 0},         {25, 0, 25}};
        return Message("character core", remote_ ? 0x00b7 : 0x00b6, [&](Buffer& b) {
            if (!b.VarU32(kCharacter) || !b.Append(appearance) || !b.U8(static_cast<uint8_t>(character_.level)) ||
                !b.U8(effectiveLevel) || !b.U8(profession))
                return false;
            if (remote_)
                return true;
            if (!b.VarU32(4815162))
                return false;
            for (const auto& row : attributes)
                for (auto value : row)
                    if (!b.U16(value))
                        return false;
            return Floats(b, {0.25f, 0.25f, 0.5f});
        });
    }
    bool CharacterState(ByteView appearance, uint8_t profession, uint8_t effectiveLevel) {
        float currentHealth = build204489::kPublishedMaximumHealth;
        if ((dialect_.build == 204489 || dialect_.build == 167409) &&
            !build204489::ReadHealthState(character_, currentHealth))
            return false;
        return Message("character definition state", 0x0292, {kCharacter, 0, 0}) &&
               Message("character affinity", dialect_.affinityId,
                       [&](Buffer& b) {
                           return b.VarU32(kCharacter) && (dialect_.compactAffinity ? b.VarU32(0) : b.U8(0));
                       }) &&
               Core(appearance, profession, effectiveLevel) &&
               Message("character flag 20", 0x00b3, [&](Buffer& b) { return b.VarU32(kCharacter) && b.U8(0); }) &&
               (remote_ || Message("character endurance", 0x00d8, {kCharacter, 100, 100, 100, 50})) &&
               Message("character health", 0x00ff, [&](Buffer& b) {
                   // Modern profiles have durable server-owned health. Do not ask the client
                   // to extrapolate passive healing that the server does not apply.
                   const float regeneration =
                       (dialect_.build == 204489 || dialect_.build == 167409) ? 0.0f : 1921.2000732421875f;
                   return b.VarU32(kCharacter) && b.U8(0) &&
                          Floats(b,
                                 {currentHealth, build204489::kPublishedMaximumHealth, regeneration, 0, 10556, 21112});
               });
    }
    bool Inventory() {
        if (!Message("inventory create", 0x0119, {kCharacter}) ||
            (!remote_ && !Message("inventory capacity", 0x0130, {kCharacter, 512})))
            return false;
        for (uint32_t i = 0; dialect_.structuralBags && i < 5; ++i) {
            if (!Message("structural bag item", 0x03c2,
                         [&](Buffer& b) {
                             return Scalars(b, {kFirstBag + i, i ? 8936u : 8932u}) && b.U8(i ? 72 : 41) && b.U8(0);
                         }) ||
                !Message("structural bag placement", 0x0120,
                         [&](Buffer& b) { return Scalars(b, {kCharacter, kFirstBag + i}) && b.U8(5) && b.VarU32(i); }))
                return false;
        }
        return Message("inventory ready", 0x011a, {kCharacter});
    }
    bool Kennel() {
        if (!Message("kennel create", 0x014b,
                     [&](Buffer& b) { return b.VarU32(kCharacter) && b.U8(6) && Zeros(b, 2); }))
            return false;
        for (uint8_t i = 0; i < 4; ++i)
            if (!Message("empty kennel slot", 0x0152, [&](Buffer& b) {
                    return b.VarU32(kCharacter) && b.U8(i) && b.Utf16Z("", 32) && Scalars(b, {0, 0});
                }))
                return false;
        return Message("kennel ready", 0x014c, {kCharacter});
    }
    template <unsigned Build> bool LegendPairs(Buffer& out) const {
        typename modern::Legends<Build>::State state;
        return state.Load(character_, 9) && modern::Legends<Build>::Pair(state, false, out) &&
               modern::Legends<Build>::Pair(state, true, out);
    }
    template <unsigned Build> bool LegendActive(Buffer& out) const {
        typename modern::Legends<Build>::State state;
        return state.Load(character_, 9) && out.VarU32(kCharacter) && out.VarU32(state.First());
    }
    bool Profession(uint8_t profession) {
        static const uint16_t ids[] = {0, 0x01dd, 0x01f8, 0x01d5, 0x01e8, 0x01f2, 0x01d9, 0x01e1, 0x01e4, 0x01ed};
        if (!Message("character profession", ids[profession], [&](Buffer& b) {
                if (!b.VarU32(kCharacter))
                    return false;
                switch (profession) {
                case 1:
                    return Floats(b, {0, 0, 0}) && Zeros(b, 2);
                // Reuse HoT's initial resource adaptation. Own receivers take maximum, regeneration and current;
                // zeroes prevent every paid skill.
                case 5:
                    return Floats(b, {8, 0, 8}) && Zeros(b, 2);
                case 2:
                    return Scalars(b, {0, 0});
                case 3:
                    return Floats(b, {0, 0, 0});
                case 4:
                    return Floats(b, {100, 0, 0}) && b.VarU32(0);
                case 6:
                    return Scalars(b, {0, 0, 0});
                case 7:
                    return Zeros(b, 2);
                case 8:
                    return Floats(b, {0, 0}) && Zeros(b, 2);
                case 9:
                    return dialect_.build == 167409 ? LegendPairs<167409>(b) : LegendPairs<204489>(b);
                default:
                    return false;
                }
            }))
            return false;
        // Resting energy cap: the native HUD doubles this maximum for its scale.
        // Regeneration starts on the first ready-world tick, after map loading.
        if (profession == 9 && !Message("revenant energy", 0x01f0,
                                        [&](Buffer& b) { return b.VarU32(kCharacter) && Floats(b, {50, 50, 0}); }))
            return false;
        return (remote_ || (Message("skillbar create", 0x026a, {kCharacter}) &&
                            Message("skillbar ready", 0x026b, {kCharacter}))) &&
               EquippedGlider() &&
               (profession != 9 || Message("active legend", 0x01f1, [&](Buffer& b) {
                    return dialect_.build == 167409 ? LegendActive<167409>(b) : LegendActive<204489>(b);
                }));
    }
    bool EquippedGlider() {
        BootstrapStep step;
        step.name = "equipped glider";
        if (!Build204489Game().Glider(glider_, remote_ ? kCharacter : 0, step.payload) || !RegionalHeader(step.payload))
            return false;
        steps.push_back(std::move(step));
        return true;
    }
    bool Managers() {
        return Message("player runtime control", 0x005f, {kPlayer, 1}) &&
               Message("player runtime clock", 0x0063,
                       [&](Buffer& b) { return b.VarU32(kPlayer) && b.U16(1) && b.U64(501); }) &&
               Message("player PvP manager", 0x0204,
                       [&](Buffer& b) {
                           return Scalars(b, {kPlayer, 0, 0}) && b.U8(0) && Scalars(b, {0, 0, 0, 0, 0, 0, 0, 0});
                       }) &&
               Message("player quest manager ready", 0x021a, {kPlayer}) &&
               Message("player reward track manager", 0x0235,
                       [&](Buffer& b) { return Scalars(b, {kPlayer, 0, 0, 0, 0}) && Zeros(b, 4); });
    }
    bool Transform(uint32_t sequence, uint8_t flags) {
        return Message("agent world transform", 0x0057, [&](Buffer& b) {
            return b.U16(kCharacter) && b.VarU32(sequence) && b.U8(flags) &&
                   Floats(b, {character_.x, character_.y, character_.z, 0}) && b.VarU32(0) && Zeros(b, 34);
        });
    }
    bool Attribute(uint8_t attribute, float value) {
        return Message("agent attribute", 0x0047, [&](Buffer& b) {
            return b.U16(kCharacter) && b.VarU32(context_.sequence) && b.U8(0x10) && b.U8(attribute) && b.F32(value);
        });
    }
    bool AgentCommands() {
        if (remote_) {
            return Message("remote controlled character", 0x01bb, {1, kPlayer, kCharacter}) &&
                   Message("remote character finalization", 0x00aa, {kCharacter}) &&
                   Message("remote collision state", 0x002e,
                           [&](Buffer& b) {
                               return b.U16(kCharacter) && b.VarU32(context_.sequence) && b.U8(0) && b.U8(1) &&
                                      b.U8(12) && Collision(b) && b.VarU32(0) && b.U16(0) && b.U16(0);
                           }) &&
                   Transform(context_.sequence, 0x90);
        }
        return Transform(context_.sequence, 0x90) &&
               Message("agent collision state", 0x002e,
                       [&](Buffer& b) {
                           return b.U16(kCharacter) && b.VarU32(context_.sequence) && b.U8(0x10) && b.U8(1) &&
                                  b.U8(14) && Collision(b) && b.VarU32(0) && b.U16(0) && b.U16(0);
                       }) &&
               Message("agent task", 0x004c,
                       [&](Buffer& b) { return b.U16(kCharacter) && b.VarU32(context_.sequence) && b.U8(0x10); }) &&
               Attribute(0, 308.6999816894531f) && Attribute(2, 1.0499999523162842f) &&
               Transform(context_.sequence + 8, 3) &&
               Message("controlled character", 0x01bb, {kPlayer, kPlayer, kCharacter});
    }
    bool Discovery(bool sectors) {
        std::vector<uint32_t> words;
        const size_t limit = sectors ? 125 : 256;
        for (const auto& pair : context_.catalog.maps) {
            const auto& ids = sectors ? pair.second.sectors : pair.second.points;
            for (uint32_t id : ids) {
                const size_t word = id >> 5;
                if (word >= limit)
                    return false;
                if (words.size() <= word)
                    words.resize(word + 1);
                words[word] |= 1u << (id & 31);
            }
        }
        return Message(sectors ? "sector discovery" : "point discovery", sectors ? 0x04ce : 0x0476, [&](Buffer& b) {
            if (!b.U16(static_cast<uint16_t>(words.size())))
                return false;
            for (auto word : words)
                if (!b.VarU32(word))
                    return false;
            return !sectors || b.U16(0);
        });
    }
    bool Wallet(uint8_t key, uint32_t value) {
        return Message("wallet currency", 0x00cb, [&](Buffer& b) { return b.U8(key) && b.VarU32(value); });
    }
    bool AccountConfiguration() {
        // Default account configuration for build 204489.
        // Word 2 bit 2 enables the rider-animation path (AcctContext flag 0x3d).
        return Message("account configuration", 0x0011, [&](Buffer& b) {
            const uint32_t values[] = {783269889, 18890892, 252, 116, 1562345473, 15, 0, 15, 15, 15};
            if (!b.U16(dialect_.accountConfigurationWords))
                return false;
            for (size_t i = 0; i < dialect_.accountConfigurationWords; ++i)
                if (!b.VarU32(values[i]))
                    return false;
            return true;
        });
    }
    bool Equipment() {
        return Message("empty primary gear loadout", 0x013c, [&](Buffer& b) {
            // Twenty records (compact zero, uint16 zero, five compact zeros), seven trailing scalars and UTF16 NUL.
            return b.U8(0) && Zeros(b, 20 * 8) && Scalars(b, {0, 0, 0, 0, 0, 0, 0}) && b.Utf16Z("", 32);
        });
    }
    bool BeginPlaying() {
        return Message("begin playing", 0x040d) && Message("post-begin owned agent", 0x0031, [&](Buffer& b) {
                   return b.U16(kCharacter) && b.VarU32(context_.sequence + 9) && b.U8(0x10);
               });
    }
    std::vector<BootstrapStep> steps;

  private:
    bool RegionalHeader(Buffer& payload) const {
        if (payload.Size() < 2)
            return false;
        const auto original =
            static_cast<uint16_t>(payload.Data()[0] | (static_cast<uint16_t>(payload.Data()[1]) << 8));
        const auto id = dialect_.Id(original);
        payload.Data()[0] = static_cast<uint8_t>(id);
        payload.Data()[1] = static_cast<uint8_t>(id >> 8);
        return true;
    }
    const BootstrapContext& context_;
    const Character& character_;
    const ModernWorldDialect& dialect_;
    uint16_t glider_;
    bool remote_;
    const uint16_t kCharacter;
    const uint32_t kPlayer, kFirstBag;
};
class Bootstrap204489 : public GameBootstrap {
  public:
    explicit Bootstrap204489(const ModernWorldDialect& dialect = {}) : dialect_(dialect) {}
    bool PeerSlot(unsigned slot, PeerIdentity& out) const override {
        if (slot >= 254)
            return false;
        out = {static_cast<uint8_t>(slot + 2), static_cast<uint16_t>(60000 + slot), 20019 + slot * 32};
        return true;
    }
    bool PeerRemove(const PeerIdentity& peer, uint32_t sequence, Buffer& out) const override {
        if (peer.player <= 1 || !peer.character || peer.character == kCharacter || !sequence)
            return false;
        Buffer message;
        if (!message.U16(dialect_.Id(0x0029)) || !message.U16(peer.character) || !message.VarU32(sequence) ||
            !message.U8(0x80) || !message.U16(dialect_.Id(0x01ad)) || !message.VarU32(peer.player))
            return false;
        out = std::move(message);
        return true;
    }
    bool Steps(const BootstrapContext& context, std::vector<BootstrapStep>& out) const override {
        return Build(context, nullptr, out);
    }
    bool PeerSteps(const BootstrapContext& context, const PeerIdentity& peer,
                   std::vector<BootstrapStep>& out) const override {
        // Reserve the owned player, character and structural bag IDs.
        if (peer.player <= 1 || !peer.character || peer.character == kCharacter || peer.firstBag < 25 ||
            peer.firstBag > UINT32_MAX - 4)
            return false;
        return Build(context, &peer, out);
    }

  private:
    ModernWorldDialect dialect_;
    bool Build(const BootstrapContext& context, const PeerIdentity* peer, std::vector<BootstrapStep>& out) const {
        const auto& character = context.entry.character;
        // Missing optional saved field inherits the existing build 204489 character default; explicit zero is invalid.
        const unsigned glider =
            character.glider ? character.glider : (character.profileFields.count("glider") ? 0u : 154u);
        Buffer summary;
        if (context.catalog.build != dialect_.build || !context.catalog.Available(character.map) || !character.map ||
            character.map > 65535 || !context.sequence || context.sequence > UINT32_MAX - 9 ||
            context.timeTicks >= 7200000 || !std::isfinite(context.timeRate) || context.timeRate < 0 || !glider ||
            glider > 65535 || !std::isfinite(character.x * 32) || !std::isfinite(character.y * 32) ||
            !std::isfinite(character.z * 32) || !Build204489Auth().Summary(character, summary))
            return false;
        unsigned effectiveLevel = character.level;
        const auto effective = character.profileFields.find("effective_level");
        if (effective != character.profileFields.end() &&
            (!ParseUnsigned(effective->second, 80, effectiveLevel) || !effectiveLevel))
            return false;
        const auto appearanceAt = character.profileFields.find("appearance");
        std::vector<uint8_t> appearance;
        if (appearanceAt == character.profileFields.end() || !DecodeHex(appearanceAt->second, 45, appearance) ||
            appearance.size() != 45)
            return false;
        const uint8_t profession = summary.Data()[5];
        Recipe recipe(context, static_cast<uint16_t>(glider), dialect_, peer);
        if (peer) {
            if (!recipe.Player() ||
                !recipe.CharacterState(ByteView(appearance.data(), appearance.size()), profession,
                                       static_cast<uint8_t>(effectiveLevel)) ||
                !recipe.Inventory() || !recipe.Profession(profession) || !recipe.AgentCommands())
                return false;
            out = std::move(recipe.steps);
            return true;
        }
        if (!recipe.MapIdentity())
            return false;
        recipe.Wait("client game hello",
                    context.entry.transfer ? GameConfirmation::TransferHello : GameConfirmation::Hello, 5000);
        if (!recipe.Time() || !recipe.LoadPosition() || !recipe.Message("map context readiness", 0x0427))
            return false;
        recipe.Wait("map context readiness", GameConfirmation::MapContextReady, 8000);
        if (!recipe.LoadMap())
            return false;
        recipe.Wait("map load accepted", GameConfirmation::MapLoadAccepted, 8000);
        if (!recipe.AccountConfiguration() || !recipe.Message("map collision scene setup", 0x041e) ||
            !recipe.Message("map agents ready", 0x0426) || !recipe.Player() ||
            !recipe.CharacterState(ByteView(appearance.data(), appearance.size()), profession,
                                   static_cast<uint8_t>(effectiveLevel)) ||
            !recipe.Inventory() || !recipe.Kennel() || !recipe.Profession(profession) || !recipe.Managers() ||
            !recipe.Message("character finalization", 0x00aa, {kCharacter}) || !recipe.AgentCommands() ||
            !recipe.Discovery(false) || !recipe.Discovery(true) ||
            !recipe.Message(
                "character health regeneration", 0x0103,
                [&](Buffer& b) {
                    return b.VarU32(kCharacter) &&
                           b.F32((dialect_.build == 204489 || dialect_.build == 167409) ? 0.0f : 2111.199951171875f);
                }) ||
            (dialect_.wallet &&
             (!recipe.Wallet(1, context.entry.account.coins) || !recipe.Wallet(2, context.entry.account.karma))) ||
            !recipe.Equipment())
            return false;
        recipe.Wait("client map loader ready", GameConfirmation::MapLoaderReady, dialect_.mapReadyTimeoutMs);
        if (!recipe.BeginPlaying())
            return false;
        out = std::move(recipe.steps);
        return true;
    }
};
} // namespace
const GameBootstrap& Build204489Bootstrap() {
    static Bootstrap204489 bootstrap;
    return bootstrap;
}
std::unique_ptr<GameBootstrap> CreateModernWorldBootstrap(const ModernWorldDialect& dialect) {
    if ((dialect.build != 204489 && dialect.build != 167409) || !dialect.affinityId || !dialect.mapReadyTimeoutMs ||
        !dialect.accountConfigurationWords || dialect.accountConfigurationWords > 10)
        return {};
    return std::unique_ptr<GameBootstrap>(new Bootstrap204489(dialect));
}
} // namespace glide
