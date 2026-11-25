#include "content.h"
#include "glide/crypto.h"
#include <cstring>
#include <cwchar>
namespace glide {
namespace july14812 {
namespace {
struct DocumentIdentity {
    const wchar_t* name;
    size_t size;
    uint8_t sha256[32];
};
const DocumentIdentity documents[] = {
#include "content_files.inc"
};
} // namespace
bool Content::Load(const std::wstring& directory, std::string& error) {
    BxmlDocuments candidate;
    if (!candidate.Load(directory, 14812, error))
        return false;
    error = "July content is incomplete or does not match its retained source package";
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
bool Content::Prepare(ByteView seed, std::string& error) {
    if (seed.size != 32) {
        error = "July content requires its native 32-byte server seed";
        return false;
    }
    return documents_.Prepare(seed, error);
}
} // namespace july14812
} // namespace glide
