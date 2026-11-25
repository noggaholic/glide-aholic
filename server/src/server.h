#pragma once
#include "glide/socket.h"
#include "glide/tls.h"
#include "glide/accounts.h"
#include "glide/config.h"
#include "glide/log.h"
#include "glide/catalog.h"
#include "glide/profile/build_profile.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace glide {
struct LoginSession {
    std::string login, userId, displayName, resumeToken;
};
enum class SelectResult { Selected, NotFound, Failed };
enum class CreateResult { Saved, Full, DuplicateName, Failed };
enum class DeleteResult { Saved, NotFound, Busy, Failed };
enum class AccountIo { Load, Save, List };
using AccountIoObserver = std::function<bool(AccountIo, const std::string&)>;
struct CommandResult;
class Server {
  public:
    explicit Server(const ServerConfig& config, std::function<uint64_t()> clock = GetTickCount64,
                    std::function<uint64_t()> wallClock = WallMilliseconds,
                    AccountIoObserver accountIo = AccountIoObserver(), LogObserver logIo = LogObserver())
        : config_(config), clock_(std::move(clock)), wallClock_(std::move(wallClock)), accountIo_(std::move(accountIo)),
          log_(std::move(logIo)) {}
    bool Start(std::string& error);
    void Run(HANDLE stop);
    uint16_t Port() const {
        return port_.load();
    }
    bool Failed() const {
        return fatal_.load();
    }
    bool Bind(uint32_t build, std::string& error);
    bool BindSts(uint32_t connectionBuild, std::string& error);
    const BuildProfile* Profile();
    const CreateFlow& Creation() const {
        return createFlow_ ? *createFlow_ : profile_->Create();
    }
    const AuthCodec& Auth() const {
        return authCodec_ ? *authCodec_ : profile_->Auth();
    }
    std::shared_ptr<const LoginSession> Login(uint64_t owner, const std::string& email, const std::string& password);
    // Performs the exchange itself: an already verified, unrelated TLS object is not accepted.
    // Salt entropy defaults to the OS random source; tests can supply independent fixtures.
    std::shared_ptr<const LoginSession> LoginSrp(uint64_t owner, TlsSession& tls, ByteView initial, std::string& error,
                                                 TlsEntropy saltEntropy = nullptr);
    std::shared_ptr<const LoginSession> Resume(uint64_t owner, const std::string& token);
    bool SessionActive(const std::shared_ptr<const LoginSession>& session);
    bool Logout(uint64_t owner, const std::shared_ptr<const LoginSession>& session);
    bool IssueGameToken(const std::shared_ptr<const LoginSession>& session, std::string& token);
    std::shared_ptr<const LoginSession> ClaimGameToken(uint64_t owner, const std::string& token);
    std::shared_ptr<const LoginSession> ClaimAuthIdentity(uint64_t owner, ByteView identity);
    bool PrepareTokenAuth(ByteView identity, std::shared_ptr<const LoginSession>& prepared, AccountState& out,
                          std::string& error);
    std::shared_ptr<const LoginSession> ClaimTokenAuthIdentity(uint64_t owner, ByteView identity,
                                                               const std::shared_ptr<const LoginSession>& prepared);
    bool LoadState(const std::shared_ptr<const LoginSession>& session, AccountState& out, std::string& error);
    bool SaveAccountConfig(const std::shared_ptr<const LoginSession>& session, ByteView bytes, std::string& error);
    bool SaveAgreement(const std::shared_ptr<const LoginSession>& session, uint8_t version, std::string& error);
    SelectResult SelectCharacter(const std::shared_ptr<const LoginSession>& session, const std::string& name,
                                 AccountState& out, std::string& error);
    CreateResult CreateCharacter(const std::shared_ptr<const LoginSession>& session, Character character,
                                 Character& out, std::string& error, uint64_t creationOwner = 0);
    DeleteResult DeleteCharacter(const std::shared_ptr<const LoginSession>& session, const Guid& character,
                                 AccountState& out, std::string& error);
    bool IssueCreate(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, uint32_t& association);
    bool IssueBoundCreate(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, unsigned slots,
                          uint32_t& association);
    std::shared_ptr<const LoginSession> ClaimBoundCreate(uint64_t owner, uint32_t association, uint32_t context,
                                                         ByteView user, ByteView character);
    std::shared_ptr<const LoginSession> ClaimGameInitializer(uint64_t owner, const GameInitializerCodec& codec,
                                                             const InitHeader& header, bool& create, WorldEntry& world);
    std::shared_ptr<const LoginSession> ClaimCreate(uint64_t owner, uint32_t association, ByteView accountGuid);
    bool IssueWorld(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, WorldEntry& out,
                    std::string& error);
    bool IssueBoundWorld(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, unsigned requestedMap,
                         uint32_t context, WorldEntry& out, std::string& error);
    std::shared_ptr<const LoginSession> ClaimBoundWorld(uint64_t owner, uint32_t association, unsigned map,
                                                        uint32_t context, ByteView user, ByteView character,
                                                        WorldEntry& out);
    bool TransferWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                       const CatalogPosition& current, unsigned map, const CatalogPosition& destination,
                       WorldEntry& out, Buffer& redirect, std::string& error);
    bool CompleteCreate(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const Guid& character,
                        Buffer& redirect, std::string& error);
    bool CompleteBoundCreate(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const Guid& character,
                             const GameInitializerCodec& codec, uint16_t port, Buffer& redirect, std::string& error);
    std::shared_ptr<const LoginSession> ClaimWorld(uint64_t owner, uint32_t association, unsigned map, uint32_t context,
                                                   ByteView prefix, WorldEntry& out);
    bool PrepareWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                      std::vector<BootstrapStep>& out, std::string& error);
    bool WorldReady(const std::shared_ptr<const LoginSession>& session, uint64_t owner, Buffer* warning = nullptr);
    std::shared_ptr<const GameMessages> WorldMessages(const std::shared_ptr<const LoginSession>& session,
                                                      uint64_t owner);
    bool SaveWorldPosition(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                           const CatalogPosition& position, std::string& error);
    bool SaveCompletedHint(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint8_t hint,
                           std::string& error);
    bool CommandReplies(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const ChatMessage& chat,
                        const CatalogPosition* position, const Facing* facing, ByteView motion,
                        std::vector<Buffer>& replies, CommandResult& command);
    bool WaypointReplies(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint32_t waypoint,
                         ByteView motion, std::vector<Buffer>& replies, CommandResult& command);
    bool DrainWorld(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                    std::vector<std::shared_ptr<const Buffer>>& messages);
    // Diagnostic only: called after the game transport attempts a queued write.
    void TraceWorldDelivery(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                            const std::shared_ptr<const Buffer>& message, bool transportAccepted);
    // The world worker consumes at most one requested transfer, using its latest live position.
    bool ProcessPendingTravel(const std::shared_ptr<const LoginSession>& session, uint64_t owner, Buffer& message,
                              bool& transferred);
    bool UpdateWorldPosition(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                             const CatalogPosition& position, CatalogPosition* authoritative = nullptr);
    bool RelayWorldMotion(const std::shared_ptr<const LoginSession>& session, uint64_t owner, ByteView motion);
    bool ApplyWorldAction(const std::shared_ptr<const LoginSession>& session, uint64_t owner, ByteView input);
    void Release(uint64_t owner);
    bool RevokePending(const std::shared_ptr<const LoginSession>& session, uint64_t issuer);
    Log& Logger() {
        return log_;
    }
    bool Verbose() const {
        return config_.verbose;
    }
    const std::wstring& DataDirectory() const {
        return config_.dataDir;
    }

  private:
    // Finish state changes before writing diagnostics. File/stdout and their
    // logger mutex are never waited on while this guard owns shared state.
    class StateGuard {
      public:
        explicit StateGuard(Server& server);
        ~StateGuard();
        StateGuard(const StateGuard&) = delete;
        StateGuard& operator=(const StateGuard&) = delete;

      private:
        Server& server_;
    };
    void QueueLogLocked(const char* component, const std::string& message);
    // Acquire before stateMutex_. An account operation includes its entire
    // read/change/save sequence; another account can use the state while I/O runs.
    class AccountOperation {
      public:
        AccountOperation(Server& server, const std::string& login);
        ~AccountOperation();
        AccountOperation(const AccountOperation&) = delete;
        AccountOperation& operator=(const AccountOperation&) = delete;

      private:
        Server& server_;
        std::string login_;
        std::shared_ptr<std::mutex> mutex_;
        std::unique_lock<std::mutex> lock_;
    };
    std::shared_ptr<std::mutex> AccountMutex(const std::string& login);
    // Caller owns stateMutex_ and the account operation. Reacquire stateMutex_
    // before returning, including failures. The store is immutable after Bind.
    bool LoadAccountLocked(const std::string& login, KvDocument& out, std::string& error);
    bool SaveAccountLocked(const KvDocument& document, std::string& error);
    bool ListAccountsLocked(std::vector<std::string>& out, std::string& error);
    class CharacterReservation {
      public:
        CharacterReservation(Server& server, std::wstring name, const Guid& identity);
        ~CharacterReservation();
        CharacterReservation(const CharacterReservation&) = delete;
        CharacterReservation& operator=(const CharacterReservation&) = delete;
        CreateResult Result() const {
            return result_;
        }

      private:
        Server& server_;
        std::wstring name_;
        std::string identity_;
        CreateResult result_ = CreateResult::Failed;
    };
    class AdmissionReservation {
      public:
        AdmissionReservation(Server& server, uint64_t owner, std::shared_ptr<const LoginSession> session);
        ~AdmissionReservation();
        AdmissionReservation(const AdmissionReservation&) = delete;
        AdmissionReservation& operator=(const AdmissionReservation&) = delete;
        bool Reserved() const {
            return reserved_;
        }

      private:
        Server& server_;
        uint64_t owner_;
        bool reserved_ = false;
    };
    std::shared_ptr<const LoginSession> FindClaimSessionLocked(uint32_t association, bool creation, bool bound);
    struct WorldTargetState {
        std::mutex mutex;
        std::condition_variable idle;
        unsigned users = 0;
        bool retiring = false;
    };
    class WorldTargetUses {
      public:
        WorldTargetUses() = default;
        ~WorldTargetUses();
        WorldTargetUses(const WorldTargetUses&) = delete;
        WorldTargetUses& operator=(const WorldTargetUses&) = delete;
        bool Pinned(const std::shared_ptr<WorldTargetState>& state,
                    const std::shared_ptr<const PeerIdentity>& identity) const;
        bool Pin(const std::shared_ptr<WorldTargetState>& state, const std::shared_ptr<const PeerIdentity>& identity);

      private:
        std::vector<std::pair<std::shared_ptr<WorldTargetState>, std::shared_ptr<const PeerIdentity>>> held_;
    };
    class WorldTargetExit {
      public:
        WorldTargetExit(Server& server, std::shared_ptr<WorldTargetState> state);
        ~WorldTargetExit();
        WorldTargetExit(const WorldTargetExit&) = delete;
        WorldTargetExit& operator=(const WorldTargetExit&) = delete;

      private:
        std::shared_ptr<WorldTargetState> state_;
    };
    void WaitWorldTargetsLocked(std::shared_ptr<WorldTargetState> state);
    struct SessionState {
        std::shared_ptr<const LoginSession> session;
        std::set<uint64_t> owners;
        std::string gameToken;
        uint64_t expires = 0, tokenExpires = 0;
        uint64_t createExpires = 0, createIssuer = 0, createOwner = 0;
        bool createSaved = false;
        Guid createdCharacter, createConfirmed;
        uint32_t createAssociation = 0;
        bool createBound = false;
        unsigned createSlots = 0;
        Guid createUser, createSelected;
        WorldEntry worldEntry;
        uint64_t worldExpires = 0, worldOwner = 0, worldIssuer = 0;
        bool worldBound = false;
        std::unique_ptr<WorldEntry> activeWorld;
        std::shared_ptr<const GameMessages> gameMessages;
        std::unique_ptr<WorldActions> worldActions;
        bool conditionDamageReady = false, conditionDamagePending = false;
        uint64_t conditionTraceOwner = 0;
        std::string conditionTrace;
        struct ConditionDelivery {
            std::shared_ptr<const Buffer> message;
            uint64_t owner = 0;
            unsigned trace = 0;
        };
        std::vector<ConditionDelivery> conditionDeliveries;
        unsigned conditionDeliveryCount = 0;
        std::shared_ptr<const PeerIdentity> peerIdentity;
        std::shared_ptr<WorldTargetState> worldTargets;
        bool peerAnnounced = false;
        bool combatEnabled = false; // Explicit opt-in, cleared when leaving this world.
        std::map<uint64_t, uint64_t> peerEffectVersions;
        std::map<uint64_t, uint64_t> peerRevealVersions;
        std::string publishedReveal;
        uint64_t revealVersion = 0;
        std::map<uint64_t, uint64_t> peerTauntVersions;
        std::string publishedTaunt;
        std::shared_ptr<const PeerIdentity> tauntCaster;
        uint64_t tauntVersion = 0;
        uint32_t forcedAttackIdentity = 0;
        Facing facing;
        bool hasFacing = false;
        Buffer controlMotion; // Last validated detailed movement, used by modern position corrections.
        uint64_t controlCorrectionAt = 0;
        bool controlCorrectionSent = false;
        Buffer flightMotion;
        uint64_t flightRelayAt = 0;
        uint16_t flightCounter = 0;
        bool flying = false;
        uint32_t mountSkin = 0, mountDefinition = 0, mountContent = 0;
        std::vector<std::shared_ptr<const Buffer>> outgoing;
        size_t outgoingBytes = 0;
        bool outgoingOverflow = false;
        unsigned requestedMap = 0;
        CatalogPosition requestedPosition;
        unsigned savedMap = 0;
        bool worldPrepared = false, worldReady = false;
        bool loggedOut = false;
    };
    struct Worker {
        std::shared_ptr<Socket> socket;
        std::shared_ptr<std::atomic<bool>> done;
        std::thread thread;
    };
    void Prune();
    SessionState* FindPendingTokenIdentityLocked(ByteView identity);
    bool IssueCreateLocked(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, uint32_t& association,
                           bool bound, unsigned slots);
    std::shared_ptr<const LoginSession> ClaimCreateLocked(uint64_t owner, uint32_t association, ByteView user,
                                                          bool bound, ByteView character,
                                                          const std::shared_ptr<const LoginSession>& expected);
    bool SaveAccountValue(const std::shared_ptr<const LoginSession>& session, const char* key, const std::string& value,
                          std::string& error);
    std::shared_ptr<const LoginSession> IssueLoginLocked(uint64_t owner, const std::string& login,
                                                         const std::string& guid, const std::string& display);
    bool IssueWorldLocked(const std::shared_ptr<const LoginSession>& session, uint64_t issuer, const Guid* created,
                          WorldEntry& out, Buffer* redirect, std::string& error, bool bound = false,
                          unsigned requestedMap = 0, uint32_t context = 0,
                          const GameInitializerCodec* initializer = nullptr, uint16_t port = 0);
    std::shared_ptr<const LoginSession> ClaimWorldLocked(uint64_t owner, uint32_t association, unsigned map,
                                                         uint32_t context, ByteView prefix, WorldEntry& out, bool bound,
                                                         ByteView user, ByteView character,
                                                         const std::shared_ptr<const LoginSession>& expectedSession);
    bool SaveWorldPositionLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                                 const CatalogPosition& position, std::string& error, bool entering = false);
    bool SaveWorldInventoryLocked(SessionState& state, const std::string& locations, const std::string& bindings,
                                  std::string& error);
    bool SaveWorldSkillRechargesLocked(SessionState& state, const std::string& deadlines, std::string& error,
                                       bool releaseState = false);
    bool SaveWorldProfessionStateLocked(SessionState& state, uint8_t selection, std::string& error,
                                        bool releaseState = false);
    bool SaveWorldCombatTraitLocked(SessionState& state, unsigned selection, std::string& error);
    bool SaveWorldLegendStateLocked(SessionState& state, const std::string& legends, std::string& error,
                                    bool releaseState = false);
    bool SaveWorldSkillStateLocked(SessionState& state, const std::string& deadlines, const std::string& effects,
                                   std::string& error, bool releaseState = false, const std::string* health = nullptr,
                                   uint8_t professionState = 0, const std::string* conditions = nullptr,
                                   const std::string* legends = nullptr, const std::string* endurance = nullptr,
                                   const std::string* initiative = nullptr, const std::string* siphon = nullptr,
                                   bool stunBreak = false, const std::string* conditionClock = nullptr);
    WorldConditionCommit SaveConditionTransferLocked(SessionState& state, const std::string& deadlines,
                                                     const std::string& effects, const std::string& health,
                                                     const std::string& conditions,
                                                     const std::vector<WorldConditionChange>& donors,
                                                     std::string& error);
    WorldActionResult TickConditionDamageLocked(SessionState& state, uint64_t utc);
    WorldConditionCommit SaveAlliedHealthLocked(SessionState& state, const WorldActionContext& context,
                                                const std::string& health,
                                                const std::vector<WorldHealthChange>& targets, std::string& error,
                                                unsigned skill = 1155, bool hostile = false, bool hostileArea = false);
    WorldConditionCommit SaveHostileEffectsLocked(SessionState&, const WorldActionContext&, unsigned, uint64_t,
                                                  const std::string&, const std::string&,
                                                  const std::vector<WorldHostileTarget>&, std::string&);
    WorldConditionCommit
    SaveAlliedEffectsLocked(SessionState& state, const WorldActionContext& context, const std::string& deadlines,
                            const std::string& effects, const std::vector<WorldReceivedEffectTarget>& targets,
                            std::vector<unsigned>& instances, std::string& error,
                            const std::function<bool(const std::vector<unsigned>&, std::string&)>& prepareOwn = {});
    bool TransferWorldLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                             const CatalogPosition& current, unsigned map, const CatalogPosition& destination,
                             WorldEntry& out, Buffer& redirect, std::string& error);
    bool SetGliderLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner, uint32_t design,
                         std::string& error);
    void MarkerRepliesLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                             const CatalogPosition& position, CommandResult& command);
    bool ApplyCommandLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner,
                            const CatalogPosition& position, ByteView motion, CommandResult output,
                            std::vector<Buffer>& replies, CommandResult& command);
    bool RelayChatLocked(const std::shared_ptr<const LoginSession>& session, uint64_t owner, const ChatMessage& chat,
                         bool announcement = false);
    bool JoinPeersLocked(SessionState& state);
    bool CombatModeLocked(SessionState& state, bool enabled);
    void LeavePeersLocked(SessionState& state);
    void QueuePeerLocked(SessionState& recipient, const std::shared_ptr<const Buffer>& message);
    void BroadcastPeerLocked(SessionState& subject, Buffer&& message, uint16_t targetAgent = 0,
                             Buffer&& targetView = Buffer(), uint64_t effectsVersion = 0,
                             const std::shared_ptr<const PeerIdentity>& extraIdentity = {});
    bool QueuePeerEffectsLocked(SessionState& recipient);
    SessionState* TauntSourceLocked(const SessionState& victim, const std::string& identity);
    bool RefreshRevealLocked(SessionState& subject);
    bool QueueRevealSnapshotLocked(SessionState& recipient, SessionState& subject);
    bool RefreshTauntLocked(SessionState& subject, bool force = false);
    bool QueueTauntSnapshotLocked(SessionState& recipient, SessionState& subject);
    WorldActionResult RunWorldActionsLocked(SessionState& state, ByteView input, bool tick, bool forcedAttack = false);
    void Serve(Socket& socket, uint64_t owner);
    ServerConfig config_;
    static uint64_t WallMilliseconds();
    std::function<uint64_t()> clock_;
    std::function<uint64_t()> wallClock_;
    AccountIoObserver accountIo_;
    Log log_;
    Socket listener_;
    std::atomic<uint16_t> port_{0};
    std::mutex stateMutex_;
    std::vector<std::pair<std::string, std::string>> stateLogs_;
    std::mutex accountMutexesMutex_;
    std::map<std::string, std::weak_ptr<std::mutex>> accountMutexes_;
    std::map<std::string, size_t> accountOperations_;
    const BuildProfile* profile_ = nullptr;
    std::unique_ptr<CreateFlow> createFlow_;
    std::unique_ptr<GameBootstrap> bootstrap_;
    const GameBootstrap& Bootstrap() const {
        return bootstrap_ ? *bootstrap_ : profile_->Bootstrap();
    }
    std::unique_ptr<CharacterCodec> characterCodec_;
    // Declared after its borrowed character codec, so it is destroyed first.
    std::unique_ptr<AuthCodec> authCodec_;
    std::unique_ptr<AccountFiles> accounts_;
    Catalog catalog_;
    uint64_t timeAnchor_ = 0;
    uint32_t timeTicks_ = 0, nextSequence_ = 1;
    std::atomic<bool> fatal_{false};
    std::map<std::string, SessionState> sessions_;
    std::vector<CharacterReservation*> pendingCharacters_;
    std::map<uint64_t, std::shared_ptr<const LoginSession>> pendingAdmissions_;
    std::map<unsigned, std::vector<std::weak_ptr<const PeerIdentity>>> peerSlots_;
    std::map<uint64_t, std::shared_ptr<Socket>> ownerSockets_;
    std::vector<Worker> workers_;
};
void RunSts(Server& server, ByteStream& socket, uint64_t owner, ByteView initial, TlsEntropy entropy = nullptr);
void RunMsgConn(Server& server, Socket& socket, uint64_t owner, ByteView initial);
} // namespace glide
