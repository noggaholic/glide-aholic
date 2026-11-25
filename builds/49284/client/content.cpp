#include "content.h"
#include "glide/files.h"
#include "glide/crypto.h"
#include <cstring>
#include <utility>

namespace glide {
namespace hot49284 {
namespace {
struct Document {
    const wchar_t* name;
    size_t size;
    uint8_t hash[32];
};

const Document documents[] = {
    {L"original-root.bin", 7164, {0xd6, 0x13, 0x8c, 0x9f, 0x1e, 0xae, 0x93, 0x77, 0x58, 0x44, 0x3e,
                                  0x96, 0xa2, 0x58, 0xeb, 0x49, 0xb6, 0xd8, 0xf5, 0x5c, 0x7f, 0x79,
                                  0xab, 0x5b, 0xf7, 0x06, 0x3f, 0xd4, 0x76, 0x17, 0xbb, 0x43}},
    {L"original-manifest-1000176.bin", 2221540, {0x8d, 0xdd, 0x58, 0xea, 0x28, 0x53, 0xe4, 0xd2, 0xc0, 0xb1, 0x2b,
                                                 0x50, 0xb8, 0xf1, 0xa1, 0xbc, 0xdf, 0xe4, 0x7d, 0xd1, 0xf0, 0x71,
                                                 0x52, 0xfa, 0xbd, 0x45, 0xfe, 0x2a, 0xb5, 0x30, 0x0d, 0x27}},
    {L"original-manifest-1000177.bin", 30296, {0xc9, 0x18, 0x51, 0x03, 0x57, 0x81, 0x65, 0x4b, 0x9f, 0x78, 0x38,
                                               0x53, 0x6d, 0xa3, 0x29, 0xaf, 0xd9, 0xbd, 0x8c, 0xd6, 0xe0, 0x45,
                                               0x9d, 0xd3, 0x6b, 0x91, 0x01, 0x8b, 0x7d, 0x13, 0x35, 0x24}},
    {L"original-manifest-1000187.bin", 1272, {0xcc, 0xa2, 0x5c, 0x8b, 0x40, 0x2b, 0x08, 0x12, 0x5f, 0xb7, 0xa4,
                                              0x73, 0xd8, 0xd3, 0xb8, 0xf1, 0x44, 0xf2, 0xa8, 0x2b, 0x75, 0x22,
                                              0x1f, 0xa1, 0x78, 0x1d, 0xa3, 0x7b, 0xcc, 0x9c, 0x5b, 0x63}},
    {L"original-manifest-1000188.bin", 264236, {0xb2, 0x74, 0xfa, 0x01, 0xc6, 0x5c, 0xd5, 0x09, 0xfc, 0xaa, 0xc8,
                                                0x3b, 0xe5, 0x83, 0xf1, 0x32, 0x38, 0x41, 0xff, 0x7d, 0x1e, 0xc6,
                                                0xc8, 0x4e, 0xf6, 0xc4, 0x91, 0x05, 0x31, 0xa1, 0x62, 0xfd}},
    {L"original-manifest-1000189.bin", 54408, {0x03, 0xfa, 0x6a, 0x7d, 0x87, 0x0d, 0xaa, 0x39, 0xe3, 0x95, 0x8b,
                                               0x43, 0xac, 0x5d, 0x8f, 0x5f, 0xa9, 0x46, 0x44, 0x9a, 0x79, 0x35,
                                               0xeb, 0xdb, 0x78, 0xbf, 0x6c, 0x2f, 0x49, 0x98, 0x97, 0x3e}}};
} // namespace
const wchar_t* Content::FileName(size_t index) {
    return index < 6 ? documents[index].name : nullptr;
}
bool Content::Publish(Glide49284PublishAssetFn asset, Glide49284PublishPropertyFn property, std::string& error) const {
    error = "HoT metadata publishers are missing";
    if (!asset || !property)
        return false;
    std::array<Glide49284InstalledManifest, 5> manifests;
    error = "HoT native manifest is missing or malformed";
    for (size_t i = 0; i < manifests.size(); ++i) {
        const ByteView bytes = Manifest(i);
        if (!glide_49284_installed_manifest(bytes.data, bytes.size, &manifests[i]))
            return false;
    }
    for (const auto& manifest : manifests)
        glide_49284_publish_manifest(&manifest, asset, property);
    error.clear();
    return true;
}
bool Content::Load(const std::wstring& directory, std::string& error) {
    std::array<Buffer, 6> candidate;
    for (size_t i = 0; i < candidate.size(); ++i) {
        const Document& expected = documents[i];
        std::string name;
        WideToUtf8(expected.name, name);
        error = "Cannot read HoT metadata: " + name;
        if (!ReadFileBytes(directory + L"/resources/" + expected.name, expected.size, candidate[i]) ||
            candidate[i].Size() != expected.size)
            return false;
        uint8_t hash[32];
        error = "HoT metadata hash mismatch: " + name;
        if (!Sha256(candidate[i].View(), hash) || std::memcmp(hash, expected.hash, sizeof(hash)))
            return false;
    }
    documents_ = std::move(candidate);
    error.clear();
    return true;
}
} // namespace hot49284
} // namespace glide
