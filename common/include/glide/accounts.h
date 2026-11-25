#pragma once
#include "glide/files.h"
#include "glide/kv.h"
#include <cstdint>
#include <mutex>
#include <vector>

namespace glide {
bool NormalizeLogin(const std::string& input, std::string& out);

// Only NotApplied permits retrying the original operation. Once the journal
// commits, a partial materialization must recover before any account is used.
enum class AccountBatchResult { NotApplied, Applied, CommittedPendingRecovery };

class AccountFiles {
  public:
    bool Open(const std::wstring& stateRoot, uint32_t build, std::string& error);
    bool Create(const std::string& email, const std::string& password, bool force, std::string& error);
    bool Load(const std::string& email, KvDocument& out, std::string& error) const;
    bool Save(const KvDocument& document, std::string& error) const;
    AccountBatchResult SaveBatch(const std::vector<KvDocument>& expected, const std::vector<KvDocument>& replacements,
                                 std::string& error) const;
    bool List(std::vector<std::string>& emails, std::string& error) const;

  private:
    bool ReadyLocked(std::string& error) const;
    bool LoadLocked(const std::string& email, KvDocument& out, std::string& error) const;
    bool RecoverBatchLocked(std::string& error) const;
    std::wstring JournalPath() const;
    bool Validate(const KvDocument& document, std::string& login, std::string& error) const;
    std::wstring Path(const std::string& normalized) const;
    DirectoryLock lock_;
    std::wstring directory_;
    uint32_t build_ = 0;
    // All public operations serialize with batch/recovery, including reads.
    // Create(force) and List call LoadLocked, not the public locking method.
    mutable std::mutex mutex_;
    mutable bool recoveryPending_ = false;
};
} // namespace glide
