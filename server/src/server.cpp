#include "server.h"
#include <Windows.h>

namespace glide {
namespace {
class UnlockedState {
  public:
    explicit UnlockedState(std::mutex& mutex) : mutex_(mutex) {
        mutex_.unlock();
    }
    ~UnlockedState() {
        mutex_.lock();
    }
    UnlockedState(const UnlockedState&) = delete;
    UnlockedState& operator=(const UnlockedState&) = delete;

  private:
    std::mutex& mutex_;
};
} // namespace
Server::StateGuard::StateGuard(Server& server) : server_(server) {
    server_.stateMutex_.lock();
}
Server::StateGuard::~StateGuard() {
    std::vector<std::pair<std::string, std::string>> records;
    records.swap(server_.stateLogs_);
    server_.stateMutex_.unlock();
    for (const auto& record : records)
        server_.log_.Write(record.first.c_str(), record.second);
}
void Server::QueueLogLocked(const char* component, const std::string& message) {
    stateLogs_.emplace_back(component, message);
}
Server::WorldTargetUses::~WorldTargetUses() {
    for (const auto& held : held_) {
        std::lock_guard<std::mutex> guard(held.first->mutex);
        if (--held.first->users == 0)
            held.first->idle.notify_all();
    }
}
bool Server::WorldTargetUses::Pinned(const std::shared_ptr<WorldTargetState>& state,
                                     const std::shared_ptr<const PeerIdentity>& identity) const {
    for (const auto& held : held_)
        if (held.first == state && held.second == identity)
            return true;
    return false;
}
bool Server::WorldTargetUses::Pin(const std::shared_ptr<WorldTargetState>& state,
                                  const std::shared_ptr<const PeerIdentity>& identity) {
    if (!state || !identity)
        return false;
    if (Pinned(state, identity))
        return true;
    std::lock_guard<std::mutex> guard(state->mutex);
    if (state->retiring || state->users == UINT32_MAX)
        return false;
    held_.emplace_back(state, identity);
    ++state->users;
    return true;
}
void Server::WaitWorldTargetsLocked(std::shared_ptr<WorldTargetState> state) {
    if (!state)
        return;
    bool wait = false;
    {
        std::lock_guard<std::mutex> guard(state->mutex);
        state->retiring = true;
        wait = state->users != 0;
    }
    if (wait) {
        UnlockedState unlocked(stateMutex_);
        std::unique_lock<std::mutex> guard(state->mutex);
        state->idle.wait(guard, [&]() { return state->users == 0; });
    }
}
Server::WorldTargetExit::WorldTargetExit(Server& server, std::shared_ptr<WorldTargetState> state)
    : state_(std::move(state)) {
    server.WaitWorldTargetsLocked(state_);
}
Server::WorldTargetExit::~WorldTargetExit() {
    if (state_) {
        std::lock_guard<std::mutex> guard(state_->mutex);
        state_->retiring = false;
    }
}
std::shared_ptr<std::mutex> Server::AccountMutex(const std::string& login) {
    std::lock_guard<std::mutex> guard(accountMutexesMutex_);
    for (auto it = accountMutexes_.begin(); it != accountMutexes_.end();) {
        if (it->second.expired())
            it = accountMutexes_.erase(it);
        else
            ++it;
    }
    auto& slot = accountMutexes_[login];
    auto mutex = slot.lock();
    if (!mutex) {
        mutex = std::make_shared<std::mutex>();
        slot = mutex;
    }
    return mutex;
}
Server::CharacterReservation::CharacterReservation(Server& server, std::wstring name, const Guid& identity)
    : server_(server), name_(std::move(name)), identity_(identity.Text(false)) {
    for (const auto* pending : server_.pendingCharacters_) {
        if (CompareStringOrdinal(name_.c_str(), -1, pending->name_.c_str(), -1, TRUE) == CSTR_EQUAL) {
            result_ = CreateResult::DuplicateName;
            return;
        }
        if (identity_ == pending->identity_)
            return;
    }
    server_.pendingCharacters_.push_back(this);
    result_ = CreateResult::Saved;
}
Server::CharacterReservation::~CharacterReservation() {
    if (result_ != CreateResult::Saved)
        return;
    for (auto at = server_.pendingCharacters_.begin(); at != server_.pendingCharacters_.end(); ++at)
        if (*at == this) {
            server_.pendingCharacters_.erase(at);
            break;
        }
}
Server::AdmissionReservation::AdmissionReservation(Server& server, uint64_t owner,
                                                   std::shared_ptr<const LoginSession> session)
    : server_(server), owner_(owner) {
    if (owner && session && !server_.pendingAdmissions_.count(owner)) {
        server_.pendingAdmissions_.emplace(owner, std::move(session));
        reserved_ = true;
    }
}
Server::AdmissionReservation::~AdmissionReservation() {
    if (reserved_)
        server_.pendingAdmissions_.erase(owner_);
}
std::shared_ptr<const LoginSession> Server::FindClaimSessionLocked(uint32_t association, bool creation, bool bound) {
    for (const auto& pair : sessions_) {
        const auto& state = pair.second;
        if (!state.loggedOut && (creation ? state.createBound == bound && state.createAssociation == association
                                          : state.worldBound == bound && state.worldEntry.association == association))
            return state.session;
    }
    return nullptr;
}
Server::AccountOperation::AccountOperation(Server& server, const std::string& login)
    : server_(server), login_(login), mutex_(server.AccountMutex(login)), lock_(*mutex_) {
    std::lock_guard<std::mutex> guard(server_.stateMutex_);
    ++server_.accountOperations_[login_];
}
Server::AccountOperation::~AccountOperation() {
    std::lock_guard<std::mutex> guard(server_.stateMutex_);
    const auto at = server_.accountOperations_.find(login_);
    if (at != server_.accountOperations_.end() && !--at->second)
        server_.accountOperations_.erase(at);
    server_.Prune();
}
bool Server::LoadAccountLocked(const std::string& login, KvDocument& out, std::string& error) {
    const auto* files = accounts_.get();
    if (!files) {
        error = "Account store is not open";
        return false;
    }
    UnlockedState unlocked(stateMutex_);
    if (accountIo_ && !accountIo_(AccountIo::Load, login)) {
        error = "Account read rejected";
        return false;
    }
    return files->Load(login, out, error);
}
bool Server::ListAccountsLocked(std::vector<std::string>& out, std::string& error) {
    const auto* files = accounts_.get();
    if (!files) {
        error = "Account store is not open";
        return false;
    }
    UnlockedState unlocked(stateMutex_);
    if (accountIo_ && !accountIo_(AccountIo::List, {})) {
        error = "Account list rejected";
        return false;
    }
    return files->List(out, error);
}
bool Server::SaveAccountLocked(const KvDocument& document, std::string& error) {
    const auto* files = accounts_.get();
    std::string email, login;
    if (!files || !document.Get("", "login", email) || !NormalizeLogin(email, login)) {
        error = "Account store or login is invalid";
        return false;
    }
    UnlockedState unlocked(stateMutex_);
    if (accountIo_ && !accountIo_(AccountIo::Save, login)) {
        error = "Account write rejected";
        return false;
    }
    return files->Save(document, error);
}
uint64_t Server::WallMilliseconds() {
    FILETIME time;
    GetSystemTimeAsFileTime(&time);
    const uint64_t ticks = (static_cast<uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
    const uint64_t epoch = 116444736000000000ULL;
    return ticks >= epoch ? (ticks - epoch) / 10000 : 0;
}
bool Server::Start(std::string& error) {
    if (!log_.Open(config_.logFile)) {
        error = "Cannot open server log";
        return false;
    }
    if (config_.build && !Bind(config_.build, error))
        return false;
    if (!listener_.Listen(config_.host, config_.port)) {
        error = "Cannot listen on configured address";
        return false;
    }
    port_.store(listener_.LocalPort());
    log_.Write("server", "Listening on " + config_.host + ":" + std::to_string(Port()));
    error.clear();
    return true;
}
bool Server::Bind(uint32_t build, std::string& error) {
    StateGuard guard(*this);
    if (fatal_) {
        error = "Server stopped after a catalog error";
        return false;
    }
    if (profile_) {
        if (profile_->Info().build == build) {
            error.clear();
            return true;
        }
        error = "This server is already assigned to another build";
        return false;
    }
    const auto* profile = BuildRegistry::Find(build);
    if (!profile) {
        error = "Build is not implemented by this server";
        return false;
    }
    Catalog catalog;
    if (!LoadCatalog(config_.dataDir + L"/" + std::to_wstring(build) + L"/catalog", build, catalog, error)) {
        QueueLogLocked("catalog", error);
        fatal_ = true;
        return false;
    }
    std::unique_ptr<CreateFlow> creator;
    if (!profile->LoadCreateFlow(config_.dataDir, creator, error))
        return false;
    std::unique_ptr<CharacterCodec> characters;
    if (!profile->LoadCharacterCodec(config_.dataDir, characters, error))
        return false;
    std::unique_ptr<AuthCodec> auth = characters ? profile->Auth().WithCharacters(*characters) : nullptr;
    std::unique_ptr<GameBootstrap> bootstrap;
    if (!profile->LoadBootstrap(config_.dataDir, bootstrap, error))
        return false;
    std::unique_ptr<AccountFiles> accounts(new AccountFiles);
    if (!accounts->Open(config_.stateDir, build, error))
        return false;
    accounts_ = std::move(accounts);
    catalog_ = std::move(catalog);
    createFlow_ = std::move(creator);
    profile_ = profile;
    characterCodec_ = std::move(characters);
    authCodec_ = std::move(auth);
    bootstrap_ = std::move(bootstrap);
    SYSTEMTIME local;
    GetLocalTime(&local);
    timeTicks_ = local.wHour * 300000u + local.wMinute * 5000u + (local.wSecond * 5000u + 30u) / 60u;
    timeAnchor_ = clock_();
    QueueLogLocked("server", "Selected build " + std::to_string(build));
    error.clear();
    return true;
}
bool Server::BindSts(uint32_t connectionBuild, std::string& error) {
    if (const auto* selected = Profile()) {
        if (connectionBuild == selected->Info().build ||
            (selected->Sts().connectionBuild && connectionBuild == selected->Sts().connectionBuild)) {
            error.clear();
            return true;
        }
        error = "STS version does not match the selected game profile";
        return false;
    }
    const BuildProfile* candidate = nullptr;
    for (size_t i = 0; i < BuildTable::Count(); ++i) {
        const auto* profile = BuildRegistry::Find(BuildTable::At(i)->build);
        if (!profile || (connectionBuild != profile->Info().build &&
                         (!profile->Sts().connectionBuild || connectionBuild != profile->Sts().connectionBuild)))
            continue;
        if (candidate) {
            error = "STS version matches several games; select --build explicitly";
            return false;
        }
        candidate = profile;
    }
    if (!candidate) {
        error = "STS version is not implemented by this server";
        return false;
    }
    return Bind(candidate->Info().build, error);
}
const BuildProfile* Server::Profile() {
    StateGuard guard(*this);
    return profile_;
}
void Server::Prune() {
    const uint64_t now = clock_();
    for (auto it = sessions_.begin(); it != sessions_.end();) {
        if (!accountOperations_.count(it->first) && it->second.owners.empty() &&
            (it->second.loggedOut || it->second.expires <= now))
            it = sessions_.erase(it);
        else
            ++it;
    }
}
void Server::Serve(Socket& socket, uint64_t owner) {
    uint8_t first[8192];
    size_t received = 0;
    if (socket.Read(first, sizeof(first), received)) {
        if (first[0] == 'P') {
            // Native June sends STS Ping every 30 seconds. The same 30-second
            // deadline races that heartbeat and disconnects a healthy game.
            if (socket.SetTimeout(90000))
                RunSts(*this, socket, owner, ByteView(first, received));
        } else
            RunMsgConn(*this, socket, owner, ByteView(first, received));
    }
    Release(owner);
}
void Server::Run(HANDLE stop) {
    uint64_t nextOwner = 0;
    while (!fatal_ && WaitForSingleObject(stop, 0) == WAIT_TIMEOUT) {
        for (auto it = workers_.begin(); it != workers_.end();) {
            if (*it->done) {
                it->thread.join();
                it = workers_.erase(it);
            } else
                ++it;
        }
        if (!listener_.WaitReadable(100))
            continue;
        auto socket = std::make_shared<Socket>();
        // Auth and game state exchange small messages whose next response can
        // depend on a client acknowledgement. Do not hold them for TCP batching.
        if (!listener_.Accept(*socket) || !socket->SetNoDelay(true) || !socket->SetTimeout(30000))
            continue;
        auto done = std::make_shared<std::atomic<bool>>(false);
        const uint64_t owner = ++nextOwner;
        {
            StateGuard guard(*this);
            ownerSockets_[owner] = socket;
        }
        std::thread thread([this, socket, done, owner] {
            Serve(*socket, owner);
            *done = true;
        });
        workers_.push_back(Worker{socket, done, std::move(thread)});
    }
    // Reject new connections before waiting for any worker's final save. Port()
    // reads the published value, never a listener handle being closed here.
    port_.store(0);
    listener_.Close();
    // Only the owning worker's shared Socket closes its handle after it is joined.
    for (auto& worker : workers_)
        worker.socket->Shutdown();
    log_.Write("server", "Stopping connections");
    for (auto& worker : workers_)
        worker.thread.join();
    workers_.clear();
    log_.Write("server", "Stopped");
}
} // namespace glide
