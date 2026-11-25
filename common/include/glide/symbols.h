#pragma once
#include "glide/pe.h"

namespace glide {
// Stable project identifier, never an instruction pattern.
struct SymbolReference {
    const char* id;
};
struct RvaSymbol {
    const char* id;
    uint32_t rva;
    uint32_t extent;
    const char* section;
    uint32_t sectionRva;
    uint32_t sectionFlags;
};
struct ExecutableProfile {
    uint32_t build;
    Bitness machine;
    const char* sha256;
    size_t fileSize;
    const RvaSymbol* symbols;
    size_t symbolCount;
};
struct SymbolMatch {
    size_t count = 0;
    uint32_t rva = 0;
};
bool MatchesExecutable(const PeImage& image, const ExecutableProfile& profile);
const ExecutableProfile* FindExecutableProfile(const PeImage& image);
const ExecutableProfile* ExecutableProfileAt(size_t index);
size_t ExecutableProfileCount();
SymbolMatch ResolveSymbol(const PeImage& image, SymbolReference reference);
// Also used by synthetic tests; the supplied profile must match the whole file.
SymbolMatch ResolveProfileSymbol(const PeImage& image, const ExecutableProfile& profile, SymbolReference reference);
} // namespace glide
