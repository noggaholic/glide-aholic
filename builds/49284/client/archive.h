#pragma once
#include "startup.h"
#include <array>
namespace glide {
const std::vector<HotSymbol>& HotArchiveSymbols();
const std::vector<HookSpec>& HotArchiveHooks();
bool ResolveHotArchive(const PeImage&, std::array<uint32_t, 3>& offsets, std::string& error);
bool PrepareHotArchive(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
