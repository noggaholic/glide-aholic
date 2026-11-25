#include "resources.h"
#include "resource_index.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <cstring>
namespace glide {
namespace july14812 {
namespace {
struct ResourceIdentity {
    const wchar_t* file;
    size_t size;
    uint8_t sha256[32];
};
const ResourceIdentity files[] = {
#include "resource_files.inc"
};
static_assert(sizeof(files) / sizeof(files[0]) == Resources::kManifestCount + 1,
              "July resource identities must cover every named map");
bool Read(const std::wstring& directory, size_t index, Buffer& out) {
    Buffer bytes;
    uint8_t digest[32];
    const auto& file = files[index];
    if (!ReadFileBytes(directory + L"/" + file.file, 16 * 1024 * 1024, bytes) || bytes.Size() != file.size ||
        !Sha256(bytes.View(), digest) || std::memcmp(digest, file.sha256, 32))
        return false;
    out = std::move(bytes);
    return true;
}
} // namespace
const wchar_t* Resources::FileName(size_t index) {
    return index < kManifestCount + 1 ? files[index].file : nullptr;
}
uint32_t Resources::PrivateId(size_t index) const {
    return index < kManifestCount && root_.Size() ? july_resource_word(root_.Data() + 48 + index * 20) : 0;
}
bool Resources::Load(const std::wstring& directory, std::string& error) {
    Buffer root;
    std::array<Buffer, kManifestCount> manifests;
    std::array<uint32_t, kManifestCount> counts{};
    error = "Missing or changed July resource root";
    if (!Read(directory, 0, root))
        return false;
    for (size_t i = 0; i < kManifestCount; ++i) {
        JulyResourceIndex manifest, view;
        error = "Missing or invalid July resource manifest";
        if (!Read(directory, i + 1, manifests[i]) ||
            !july_resource_manifest(manifests[i].Data(), manifests[i].Size(), &manifest) ||
            !july_resource_root(root.Data(), root.Size(), manifests[i].Size(), &view) || view.count != kManifestCount)
            return false;
        counts[i] = manifest.count;
    }
    root_ = std::move(root);
    manifests_ = std::move(manifests);
    counts_ = counts;
    error.clear();
    return true;
}
} // namespace july14812
} // namespace glide
