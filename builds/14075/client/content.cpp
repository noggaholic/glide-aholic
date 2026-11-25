#include "content.h"
#include "glide/crypto.h"
#include <cstring>

namespace glide {
namespace april14075 {
namespace {
struct DocumentIdentity {
    const wchar_t* name;
    size_t size;
    uint8_t sha256[32];
};
const DocumentIdentity documents[] = {
#include "content_files.inc"
};
const char key[] = "GLIDE_AHOLIC_CONTENT";
static_assert(sizeof(key) - 1 == 20, "Retained April RC4 key must be 20 bytes");
} // namespace
ByteView Content::Key() {
    return ByteView(key, sizeof(key) - 1);
}
bool Content::Load(const std::wstring& directory, std::string& error) {
    BxmlDocuments candidate;
    if (!candidate.Load(directory, 14075, error, BxmlDocumentNames::Named))
        return false;
    error = "April content is incomplete or differs from the verified own documents";
    if (candidate.Count() != sizeof(documents) / sizeof(documents[0]) ||
        !candidate.VisitPlaintext([](const std::wstring& name, ByteView bytes) {
            for (const auto& identity : documents)
                if (name == identity.name) {
                    uint8_t digest[32];
                    return bytes.size == identity.size && Sha256(bytes, digest) &&
                           !std::memcmp(digest, identity.sha256, 32);
                }
            return false;
        }))
        return false;
    documents_ = std::move(candidate);
    error.clear();
    return true;
}
bool Content::Prepare(std::string& error) {
    return documents_.PrepareKey(Key(), error);
}
} // namespace april14075
} // namespace glide
