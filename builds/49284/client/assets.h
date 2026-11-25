#pragma once
#include "startup.h"
#include <map>
namespace glide {
class HotAssets {
  public:
    bool Load(const std::wstring& directory, std::string& error);
    ByteView Find(unsigned id, unsigned variant) const;
    size_t Count() const {
        return files_.size();
    }

  private:
    std::map<unsigned, Buffer> files_;
};
using HotAssetChunk = unsigned(__fastcall*)(const void*, unsigned, unsigned, unsigned, unsigned, void*);
int ReadHotAsset(ByteView asset, unsigned* job, HotAssetChunk chunk);
const std::vector<HotSymbol>& HotAssetSymbols();
const std::vector<HookSpec>& HotAssetHooks();
bool PrepareHotAssets(ClientRuntime&, const PeImage&, void* module, std::string& error);
} // namespace glide
