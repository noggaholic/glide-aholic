#include "glide/bxml_documents.h"
#include "glide/bxml.h"
#include "glide/files.h"
#include "glide/rc4.h"
#include <Windows.h>
#include <algorithm>
#include <sstream>
namespace glide {
namespace {
std::wstring CanonicalName(std::wstring name, BxmlDocumentNames mode) {
    if (mode == BxmlDocumentNames::Named)
        for (auto& letter : name)
            if (letter >= L'A' && letter <= L'Z')
                letter += L'a' - L'A';
    return name;
}
bool BuildMetadata(const std::wstring& path, uint32_t expected) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;
    char prefix[128];
    DWORD received = 0;
    const BOOL read = ReadFile(file, prefix, sizeof(prefix), &received, nullptr);
    CloseHandle(file);
    if (!read || !received || !expected)
        return false;
    std::istringstream input(std::string(prefix, received));
    char token;
    if (!(input >> token) || token != '{' || !(input >> token) || token != '"')
        return false;
    std::string key;
    if (!std::getline(input, key, '"') || key != "build" || !(input >> token) || token != ':')
        return false;
    input >> std::ws;
    if (input.peek() < '0' || input.peek() > '9')
        return false;
    unsigned build = 0;
    return (input >> build >> token) && build == expected && (token == ',' || token == '}');
}
} // namespace
bool BxmlDocuments::Load(const std::wstring& directory, uint32_t build, std::string& error, BxmlDocumentNames names) {
    error = "Unsupported content document name policy";
    if (names != BxmlDocumentNames::Hashed && names != BxmlDocumentNames::Named)
        return false;
    Buffer index;
    error = "Missing or mismatched content build metadata";
    if (!BuildMetadata(directory + L"/index.json", build))
        return false;
    error = "Cannot read content document list";
    if (!ReadFileBytes(directory + L"/documents.txt", 32768, index) || !index.Size())
        return false;
    std::map<std::wstring, Buffer> documents;
    std::istringstream input(std::string(reinterpret_cast<const char*>(index.Data()), index.Size()));
    std::string line;
    size_t total = 0;
    const size_t limit = 16 * 1024 * 1024;
    while (std::getline(input, line)) {
        std::istringstream row(line);
        std::string name, basename, extra;
        error = "Malformed content document list";
        if (line.size() > 127 || !(row >> name >> basename) || (row >> extra) || documents.size() >= 256 ||
            basename.empty() || basename.size() > 63 ||
            basename.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789-") != std::string::npos)
            return false;
        if (name != "@manifest") {
            const bool hashed = names == BxmlDocumentNames::Hashed;
            const char* alphabet = hashed ? "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
                                          : "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
            if ((hashed ? name.size() != 5 : (name.empty() || name.size() > 63)) ||
                name.find_first_not_of(alphabet) != std::string::npos)
                return false;
        }
        const std::wstring wideName = CanonicalName(std::wstring(name.begin(), name.end()), names);
        const std::wstring file(basename.begin(), basename.end());
        if (documents.count(wideName))
            return false;
        Buffer bytes;
        error = "Cannot read content document: " + basename;
        if (!ReadFileBytes(directory + L"/" + file + L".bxml", limit - total, bytes))
            return false;
        if (!ValidateBxml(bytes.View(), error)) {
            error = basename + ": " + error;
            return false;
        }
        total += bytes.Size();
        documents.emplace(wideName, std::move(bytes));
    }
    error = "Content requires a manifest and at least one document";
    if (input.bad() || documents.size() < 2 || !documents.count(L"@manifest"))
        return false;
    documents_ = std::move(documents);
    key_.fill(0);
    names_ = names;
    prepared_ = false;
    error.clear();
    return true;
}
bool BxmlDocuments::Prepare(ByteView seed, std::string& error) {
    std::array<uint8_t, 20> key;
    error = "Content is not loaded or the server seed is invalid";
    if (documents_.empty() || seed.size > 1024 || !DeriveContentKey(seed, key.data()))
        return false;
    return PrepareKey(ByteView(key.data(), key.size()), error);
}
bool BxmlDocuments::PrepareKey(ByteView input, std::string& error) {
    error = "Content is not loaded or the RC4 key is invalid";
    if (documents_.empty() || !input.data || input.size != key_.size())
        return false;
    std::array<uint8_t, 20> key;
    std::copy(input.data, input.data + input.size, key.begin());
    if (prepared_ && key == key_) {
        error.clear();
        return true;
    }
    for (auto& entry : documents_) {
        Rc4 previous, next;
        if (prepared_) {
            previous.Initialize(ByteView(key_.data(), key_.size()));
            previous.Apply(entry.second.Data(), entry.second.Size());
        }
        next.Initialize(ByteView(key.data(), key.size()));
        next.Apply(entry.second.Data(), entry.second.Size());
    }
    key_ = key;
    prepared_ = true;
    error.clear();
    return true;
}
ByteView BxmlDocuments::Document(const wchar_t* name) const {
    if (!prepared_)
        return {};
    const auto at = documents_.find(CanonicalName(name ? name : L"@manifest", names_));
    return at == documents_.end() ? ByteView() : at->second.View();
}
bool BxmlDocuments::VisitPlaintext(const std::function<bool(const std::wstring&, ByteView)>& visitor) const {
    if (prepared_ || documents_.empty() || !visitor)
        return false;
    for (const auto& entry : documents_)
        if (!visitor(entry.first, entry.second.View()))
            return false;
    return true;
}
bool BxmlDocuments::ReplacePlaintext(const wchar_t* name, Buffer&& replacement, std::string& error) {
    error = "Content replacement requires an existing unencrypted document";
    if (prepared_)
        return false;
    const auto at = documents_.find(CanonicalName(name ? name : L"@manifest", names_));
    if (at == documents_.end())
        return false;
    size_t total = 0;
    for (const auto& entry : documents_)
        if (&entry != &*at)
            total += entry.second.Size();
    if (replacement.Size() > 16 * 1024 * 1024 - total) {
        error = "Content replacement exceeds catalog limit";
        return false;
    }
    if (!ValidateBxml(replacement.View(), error))
        return false;
    at->second = std::move(replacement);
    error.clear();
    return true;
}
} // namespace glide
