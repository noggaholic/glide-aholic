#include "glide/accounts.h"
#include "glide/builds.h"
#include "glide/crypto.h"
#include <Windows.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <set>

namespace glide {
namespace {
constexpr size_t kAccountLimit = 4u * 1024u * 1024u;
// Caster plus five allies for effects that exclude the caster.
constexpr unsigned kBatchAccounts = 6;
constexpr size_t kBatchLimit = kBatchAccounts * (kAccountLimit + 230u) + 48u;
const uint8_t kBatchMagic[] = {'G', 'A', 'B', 'A', 'T', 'C', 'H', '1'};
struct BatchEntry {
    std::string login, text;
    std::array<uint8_t, 32> expected{};
    KvDocument replacement;
    bool write = false;
};
bool GuidText(const std::string& value) {
    if (value.size() != 32)
        return false;
    for (char c : value)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    return true;
}
// Batches change state, not account/character identities or the selected slot.
// Build-specific field semantics remain the caller's profile validation.
bool SameIdentity(const KvDocument& before, const KvDocument& after) {
    std::string a, b;
    if (!before.Get("", "user_guid", a) || !after.Get("", "user_guid", b) || a != b || !GuidText(a) ||
        !before.Get("", "selected", a) || !after.Get("", "selected", b) || a != b)
        return false;
    unsigned selected = 0;
    if (!ParseUnsigned(a, UINT32_MAX, selected))
        return false;
    auto left = before.Sections(), right = after.Sections();
    std::sort(left.begin(), left.end());
    std::sort(right.begin(), right.end());
    if (left != right || std::adjacent_find(left.begin(), left.end()) != left.end())
        return false;
    for (const auto& section : left)
        if (section.compare(0, 10, "character ") == 0) {
            unsigned slot = 0;
            if (!ParseUnsigned(section.substr(10), UINT32_MAX, slot) || !before.Get(section, "guid", a) ||
                !after.Get(section, "guid", b) || a != b || !GuidText(a))
                return false;
        }
    return true;
}
bool MissingFile(DWORD error) {
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
}
} // namespace
bool NormalizeLogin(const std::string& input, std::string& out) {
    if (input.empty() || input.size() > 190)
        return false;
    const size_t at = input.find('@');
    if (at == std::string::npos || at == 0 || at + 1 == input.size() || input.find('@', at + 1) != std::string::npos)
        return false;
    std::string normalized;
    for (char c : input) {
        if (c >= 'A' && c <= 'Z')
            c = static_cast<char>(c - 'A' + 'a');
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '+' ||
              c == '@'))
            return false;
        normalized += c;
    }
    out = std::move(normalized);
    return true;
}
bool AccountFiles::Open(const std::wstring& stateRoot, uint32_t build, std::string& error) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (lock_.Held()) {
        error = "Account store is already open";
        return false;
    }
    if (!BuildTable::Find(build)) {
        error = "Unsupported build";
        return false;
    }
    const std::wstring directory = stateRoot + L"/" + std::to_wstring(build);
    if (!EnsureDirectory(directory)) {
        error = "Cannot create state directory";
        return false;
    }
    if (!lock_.Acquire(directory)) {
        error = "State is in use or cannot be locked; stop its server before editing accounts";
        return false;
    }
    if (!EnsureDirectory(directory + L"/accounts")) {
        lock_.Release();
        error = "Cannot create accounts directory";
        return false;
    }
    directory_ = directory + L"/accounts";
    build_ = build;
    recoveryPending_ = true;
    // Keep the directory lock and block this object on failure. Reopening a new
    // store after the obstruction is removed retries the committed journal.
    return RecoverBatchLocked(error);
}
std::wstring AccountFiles::Path(const std::string& normalized) const {
    return directory_ + L"/" + std::wstring(normalized.begin(), normalized.end()) + L".cfg";
}
bool AccountFiles::Validate(const KvDocument& document, std::string& login, std::string& error) const {
    std::string format, build, email, guid, password;
    unsigned number = 0;
    if (!document.Get("", "format", format) || format != "1" || !document.Get("", "build", build) ||
        !ParseUnsigned(build, UINT32_MAX, number) || number != build_ || !document.Get("", "login", email) ||
        !NormalizeLogin(email, login) || !document.Get("", "password", password) || password.empty() ||
        !document.Get("", "user_guid", guid) || guid.size() != 32) {
        error = "Invalid account header or mismatched build";
        return false;
    }
    for (char c : guid)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            error = "Invalid account GUID";
            return false;
        }
    return true;
}
bool AccountFiles::Load(const std::string& email, KvDocument& out, std::string& error) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return ReadyLocked(error) && LoadLocked(email, out, error);
}
bool AccountFiles::LoadLocked(const std::string& email, KvDocument& out, std::string& error) const {
    std::string login;
    if (!lock_.Held() || !NormalizeLogin(email, login)) {
        error = "Invalid login or closed account store";
        return false;
    }
    Buffer contents;
    if (!ReadFileBytes(Path(login), 4 * 1024 * 1024, contents)) {
        error = "Cannot read account";
        return false;
    }
    KvDocument document;
    const std::string text(contents.Size() ? reinterpret_cast<const char*>(contents.Data()) : "", contents.Size());
    if (!KvDocument::Parse(text, document, error))
        return false;
    std::string storedLogin;
    if (!Validate(document, storedLogin, error))
        return false;
    if (storedLogin != login) {
        error = "Account filename does not match login";
        return false;
    }
    out = std::move(document);
    error.clear();
    return true;
}
bool AccountFiles::Save(const KvDocument& document, std::string& error) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!ReadyLocked(error))
        return false;
    std::string login;
    if (!Validate(document, login, error))
        return false;
    const std::string text = document.Serialize();
    if (!WriteFileAtomic(Path(login), ByteView(text.data(), text.size()), true)) {
        error = "Cannot save account; previous file retained";
        return false;
    }
    error.clear();
    return true;
}
bool AccountFiles::Create(const std::string& email, const std::string& password, bool force, std::string& error) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!ReadyLocked(error))
        return false;
    std::string login;
    if (!lock_.Held() || !NormalizeLogin(email, login)) {
        error = "Invalid email or closed account store";
        return false;
    }
    if (password.empty() || password.size() > 4096 || password.find_first_of("\r\n") != std::string::npos ||
        password.find('\0') != std::string::npos) {
        error = "Password must be nonempty single-line text";
        return false;
    }
    std::wstring wide;
    if (!Utf8ToWide(password, wide)) {
        error = "Password must be UTF-8";
        return false;
    }
    KvDocument document;
    const bool exists = GetFileAttributesW(Path(login).c_str()) != INVALID_FILE_ATTRIBUTES;
    if (exists) {
        if (!force) {
            error = "Account exists; --force updates its password without deleting characters";
            return false;
        }
        if (!LoadLocked(login, document, error))
            return false;
    } else {
        uint8_t bytes[16];
        if (!RandomBytes(bytes, sizeof(bytes))) {
            error = "Cannot generate account identity";
            return false;
        }
        const char* hex = "0123456789abcdef";
        std::string guid;
        for (uint8_t b : bytes) {
            guid += hex[b >> 4];
            guid += hex[b & 15];
        }
        const std::string initial = "# Glide Aholic account\nformat=1\nbuild=" + std::to_string(build_) +
                                    "\nlogin=" + login + "\npassword=" + password +
                                    "\ndisplay_name=" + login.substr(0, login.find('@')) + "\nuser_guid=" + guid +
                                    "\ncoins=999999\nkarma=99999\nhome_world=2102\naccess_level=2\nselected=0\n";
        if (!KvDocument::Parse(initial, document, error))
            return false;
    }
    if (!document.Set("", "password", password)) {
        error = "Cannot update password";
        return false;
    }
    const std::string text = document.Serialize();
    if (!WriteFileAtomic(Path(login), ByteView(text.data(), text.size()), exists)) {
        error = "Cannot save account; previous file retained";
        return false;
    }
    error.clear();
    return true;
}
bool AccountFiles::List(std::vector<std::string>& emails, std::string& error) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!ReadyLocked(error))
        return false;
    WIN32_FIND_DATAW data;
    HANDLE search = FindFirstFileW((directory_ + L"/*.cfg").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) {
        if (GetLastError() == ERROR_FILE_NOT_FOUND) {
            emails.clear();
            error.clear();
            return true;
        }
        error = "Cannot enumerate accounts";
        return false;
    }
    struct Close {
        HANDLE value;
        ~Close() {
            FindClose(value);
        }
    } close{search};
    std::vector<std::string> result;
    do {
        if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            continue;
        std::wstring name(data.cFileName);
        if (name.size() < 4)
            continue;
        name.resize(name.size() - 4);
        std::string email;
        if (!WideToUtf8(name, email)) {
            error = "Invalid account filename";
            return false;
        }
        KvDocument document;
        if (!LoadLocked(email, document, error))
            return false;
        result.push_back(email);
    } while (FindNextFileW(search, &data));
    if (GetLastError() != ERROR_NO_MORE_FILES) {
        error = "Account enumeration failed";
        return false;
    }
    std::sort(result.begin(), result.end());
    emails = std::move(result);
    error.clear();
    return true;
}
std::wstring AccountFiles::JournalPath() const {
    return directory_ + L"/.account-batch-v1";
}
bool AccountFiles::ReadyLocked(std::string& error) const {
    if (!lock_.Held()) {
        error = "Account store is not open";
        return false;
    }
    if (!recoveryPending_) {
        const DWORD attributes = GetFileAttributesW(JournalPath().c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES || !MissingFile(GetLastError()))
            recoveryPending_ = true;
    }
    if (recoveryPending_) {
        error = "Account batch recovery is required; account access blocked";
        return false;
    }
    return true;
}
bool AccountFiles::RecoverBatchLocked(std::string& error) const {
    const auto journal = JournalPath();
    const DWORD attributes = GetFileAttributesW(journal.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES && MissingFile(GetLastError())) {
        recoveryPending_ = false;
        error.clear();
        return true;
    }
    recoveryPending_ = true;
    error = "Account batch journal is invalid or unavailable; account access blocked";
    if (attributes == INVALID_FILE_ATTRIBUTES ||
        (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
        return false;
    Buffer bytes;
    if (!ReadFileBytes(journal, kBatchLimit, bytes) || bytes.Size() < sizeof(kBatchMagic) + 8 + 32)
        return false;
    const size_t payloadSize = bytes.Size() - 32;
    uint8_t digest[32];
    if (!Sha256(ByteView(bytes.Data(), payloadSize), digest) || std::memcmp(digest, bytes.Data() + payloadSize, 32))
        return false;
    Reader reader(ByteView(bytes.Data(), payloadSize));
    ByteView field;
    uint32_t build = 0, count = 0;
    if (!reader.Bytes(sizeof(kBatchMagic), field) || std::memcmp(field.data, kBatchMagic, sizeof(kBatchMagic)) ||
        !reader.U32(build) || build != build_ || !reader.U32(count) || !count || count > kBatchAccounts)
        return false;
    std::vector<BatchEntry> entries;
    std::set<std::string> logins;
    for (uint32_t i = 0; i < count; ++i) {
        BatchEntry entry;
        uint32_t length = 0;
        if (!reader.U32(length) || !length || length > 190 || !reader.Bytes(length, field))
            return false;
        entry.login.assign(reinterpret_cast<const char*>(field.data), field.size);
        std::string normalized;
        if (!NormalizeLogin(entry.login, normalized) || normalized != entry.login ||
            !logins.insert(entry.login).second || !reader.Bytes(32, field))
            return false;
        std::memcpy(entry.expected.data(), field.data, 32);
        if (!reader.U32(length) || !length || length > kAccountLimit || !reader.Bytes(length, field))
            return false;
        entry.text.assign(reinterpret_cast<const char*>(field.data), field.size);
        std::string detail, login;
        if (!KvDocument::Parse(entry.text, entry.replacement, detail) || !Validate(entry.replacement, login, detail) ||
            login != entry.login || entry.replacement.Serialize() != entry.text)
            return false;
        entries.push_back(std::move(entry));
    }
    if (reader.Remaining())
        return false;
    // Validate every image and current account before the first replacement.
    // Old or already-applied images are allowed; unrelated external edits block
    // recovery instead of being silently overwritten.
    for (auto& entry : entries) {
        KvDocument current;
        std::string detail;
        if (!LoadLocked(entry.login, current, detail) || !SameIdentity(current, entry.replacement))
            return false;
        const auto text = current.Serialize();
        uint8_t currentHash[32], replacementHash[32];
        if (text.size() > kAccountLimit || !Sha256(ByteView(text.data(), text.size()), currentHash) ||
            !Sha256(ByteView(entry.text.data(), entry.text.size()), replacementHash))
            return false;
        entry.write = std::memcmp(currentHash, replacementHash, 32) != 0;
        if (entry.write && std::memcmp(currentHash, entry.expected.data(), 32))
            return false;
    }
    for (const auto& entry : entries)
        if (entry.write && !WriteFileAtomic(Path(entry.login), ByteView(entry.text.data(), entry.text.size()), true)) {
            error = "Committed account batch requires recovery; account access blocked";
            return false;
        }
    if (!DeleteFileW(journal.c_str())) {
        error = "Committed account batch journal cannot be retired; account access blocked";
        return false;
    }
    recoveryPending_ = false;
    error.clear();
    return true;
}
AccountBatchResult AccountFiles::SaveBatch(const std::vector<KvDocument>& expected,
                                           const std::vector<KvDocument>& replacements, std::string& error) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!ReadyLocked(error))
        return recoveryPending_ ? AccountBatchResult::CommittedPendingRecovery : AccountBatchResult::NotApplied;
    error = "Invalid account batch or conflicting saved state";
    if (expected.empty() || expected.size() > kBatchAccounts || expected.size() != replacements.size())
        return AccountBatchResult::NotApplied;
    Buffer journal;
    std::set<std::string> logins;
    if (!journal.Append(ByteView(kBatchMagic, sizeof(kBatchMagic))) || !journal.U32(build_) ||
        !journal.U32(static_cast<uint32_t>(expected.size())))
        return AccountBatchResult::NotApplied;
    for (size_t i = 0; i < expected.size(); ++i) {
        std::string beforeLogin, afterLogin, detail;
        KvDocument current, before, after;
        const auto beforeText = expected[i].Serialize(), afterText = replacements[i].Serialize();
        if (beforeText.empty() || beforeText.size() > kAccountLimit || afterText.empty() ||
            afterText.size() > kAccountLimit || !KvDocument::Parse(beforeText, before, detail) ||
            !KvDocument::Parse(afterText, after, detail) || !Validate(before, beforeLogin, detail) ||
            !Validate(after, afterLogin, detail) || beforeLogin != afterLogin || !logins.insert(beforeLogin).second ||
            !SameIdentity(before, after) || !LoadLocked(beforeLogin, current, detail) ||
            current.Serialize() != beforeText)
            return AccountBatchResult::NotApplied;
        uint8_t digest[32];
        if (!Sha256(ByteView(beforeText.data(), beforeText.size()), digest) ||
            !journal.U32(static_cast<uint32_t>(beforeLogin.size())) ||
            !journal.Append(ByteView(beforeLogin.data(), beforeLogin.size())) ||
            !journal.Append(ByteView(digest, 32)) || !journal.U32(static_cast<uint32_t>(afterText.size())) ||
            !journal.Append(ByteView(afterText.data(), afterText.size())))
            return AccountBatchResult::NotApplied;
    }
    uint8_t digest[32];
    if (!Sha256(journal.View(), digest) || !journal.Append(ByteView(digest, 32)) || journal.Size() > kBatchLimit)
        return AccountBatchResult::NotApplied;
    if (!WriteFileAtomic(JournalPath(), journal.View(), false)) {
        error = "Account batch journal was not committed";
        return AccountBatchResult::NotApplied;
    }
    // This is the single durable commit point. Never return NotApplied after it.
    recoveryPending_ = true;
    return RecoverBatchLocked(error) ? AccountBatchResult::Applied : AccountBatchResult::CommittedPendingRecovery;
}
} // namespace glide
