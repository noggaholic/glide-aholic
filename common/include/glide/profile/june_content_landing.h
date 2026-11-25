#pragma once
#include "glide/bxml_documents.h"
#include "glide/crypto.h"
#include "glide/profile/june_water_landing_data.h"
#include <cstring>

namespace glide {
namespace beta {
inline bool BuildWaterLanding(const WaterLandingPatch& patch, ByteView source, Buffer& out, std::string& error) {
    uint8_t digest[32];
    error = "June water landing source identity differs";
    if (source.size != patch.inputBytes || !Sha256(source, digest) || std::memcmp(digest, patch.inputHash, 32))
        return false;
    if (source.size < 10 || source.size > 16 * 1024 * 1024 || patch.insertOffset > source.size ||
        patch.childOffset >= source.size || patch.poolOffset >= source.size ||
        source.data[patch.childOffset] != patch.children || patch.children >= 255 ||
        patch.outputBytes != patch.inputBytes + 38)
        return false;
    Buffer next;
    if (!next.Append(ByteView(source.data, patch.insertOffset)) ||
        !next.Append(ByteView(patch.record, sizeof(patch.record))) ||
        !next.Append(ByteView(source.data + patch.insertOffset, source.size - patch.insertOffset)))
        return false;
    ++next.Data()[patch.childOffset];
    const uint32_t pool = patch.poolOffset + static_cast<uint32_t>(sizeof(patch.record));
    for (unsigned i = 0; i < 4; ++i)
        next.Data()[6 + i] = static_cast<uint8_t>(pool >> (8 * i));
    if (next.Size() != patch.outputBytes || !Sha256(next.View(), digest) || std::memcmp(digest, patch.outputHash, 32))
        return false;
    out = std::move(next);
    error.clear();
    return true;
}
inline bool ApplyWaterLanding(unsigned build, BxmlDocuments& documents, std::string& error) {
    const auto* patch = FindWaterLandingPatch(build);
    if (!patch)
        return false;
    Buffer replacement;
    bool found = false, already = false;
    if (!documents.VisitPlaintext([&](const std::wstring& name, ByteView bytes) {
            if (name != patch->name)
                return true;
            found = true;
            uint8_t digest[32];
            already = bytes.size == patch->outputBytes && Sha256(bytes, digest) &&
                      !std::memcmp(digest, patch->outputHash, 32);
            return already || BuildWaterLanding(*patch, bytes, replacement, error);
        }))
        return false;
    // A structural test catalog may omit this namespace. A matching namespace
    // must have one of the authenticated own input/output identities.
    if (!found || already) {
        error.clear();
        return true;
    }
    return documents.ReplacePlaintext(patch->name, std::move(replacement), error);
}
} // namespace beta
} // namespace glide
