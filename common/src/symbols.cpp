#include "glide/symbols.h"
#include <cstring>

namespace glide {
namespace {
int Hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}
} // namespace
bool MatchesExecutable(const PeImage& image, const ExecutableProfile& profile) {
    if (!profile.sha256 || std::strlen(profile.sha256) != 64 || profile.machine != image.Machine() ||
        profile.fileSize != image.SourceSize())
        return false;
    for (size_t i = 0; i < image.Fingerprint().size(); ++i) {
        const int high = Hex(profile.sha256[2 * i]), low = Hex(profile.sha256[2 * i + 1]);
        if (high < 0 || low < 0 || image.Fingerprint()[i] != (high * 16 + low))
            return false;
    }
    return true;
}
const ExecutableProfile* FindExecutableProfile(const PeImage& image) {
    const ExecutableProfile* result = nullptr;
    for (size_t i = 0; i < ExecutableProfileCount(); ++i) {
        const auto* candidate = ExecutableProfileAt(i);
        if (MatchesExecutable(image, *candidate)) {
            if (result)
                return nullptr;
            result = candidate;
        }
    }
    return result;
}
SymbolMatch ResolveProfileSymbol(const PeImage& image, const ExecutableProfile& profile, SymbolReference reference) {
    if (!reference.id || !profile.symbols || !MatchesExecutable(image, profile))
        return {};
    const RvaSymbol* found = nullptr;
    for (size_t i = 0; i < profile.symbolCount; ++i) {
        if (!std::strcmp(profile.symbols[i].id, reference.id)) {
            if (found)
                return {};
            found = &profile.symbols[i];
        }
    }
    if (!found || !found->extent || !found->section)
        return {};
    for (const auto& section : image.Code()) {
        if (section.rva != found->sectionRva || section.characteristics != found->sectionFlags ||
            std::strcmp(section.name.data(), found->section) || !(section.characteristics & 0x20000000u) ||
            found->rva < section.rva)
            continue;
        const size_t offset = found->rva - section.rva;
        if (offset < section.bytes.size && found->extent <= section.bytes.size - offset)
            return {1, found->rva};
    }
    return {};
}
SymbolMatch ResolveSymbol(const PeImage& image, SymbolReference reference) {
    const auto* profile = FindExecutableProfile(image);
    return profile ? ResolveProfileSymbol(image, *profile, reference) : SymbolMatch{};
}
} // namespace glide
