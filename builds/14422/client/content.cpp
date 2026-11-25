#include "content.h"
#include "glide/crypto.h"
#include "glide/profile/june_content_landing.h"
#include <cstring>

namespace glide {
namespace june14422 {
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
    if (!candidate.Load(directory, 14422, error))
        return false;
    error = "June 7 content is incomplete or differs from its retained source package";
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
    if (!beta::ApplyWaterLanding(14422, candidate, error))
        return false;
    documents_ = std::move(candidate);
    error.clear();
    return true;
}
bool Content::Prepare(ByteView seed, std::string& error) {
    return documents_.Prepare(seed, error);
}
} // namespace june14422
} // namespace glide
