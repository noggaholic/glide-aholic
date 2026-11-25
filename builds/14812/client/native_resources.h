#pragma once
#include "resources.h"
#include "glide/symbols.h"
namespace glide {
namespace july14812 {
// Exact retained x86 native ABI, including the asynchronous archive operations.
struct ResourceApi {
    using Pack = void*(__fastcall*)(unsigned, unsigned, const void*);
    using Stage = void(__fastcall*)(void*, void*);
    using Chunk = void*(__fastcall*)(void*, void*, unsigned, unsigned, unsigned*);
    using Write = void(__fastcall*)(void*, void*, unsigned, unsigned, unsigned, unsigned, const void*, void**, void*,
                                    unsigned);
    using Operation = unsigned char(__fastcall*)(void*, void*);
    using FindIds = void(__fastcall*)(void*, void*, const unsigned*, unsigned*, unsigned, void**, void*, unsigned);
    using Size = void(__fastcall*)(unsigned, unsigned, unsigned);
    using Query = int(__fastcall*)(unsigned, unsigned, unsigned*, unsigned*);
    Pack pack = nullptr;
    Stage registerManifest = nullptr, registerRoot = nullptr, publishRoot = nullptr;
    Write write = nullptr;
    Operation complete = nullptr, failed = nullptr;
    FindIds findIds = nullptr;
    Size publishSize = nullptr;
    Query query = nullptr;
};
struct ResourceSymbol {
    const char* name;
    SymbolReference symbol;
};
const std::vector<ResourceSymbol>& ResourceSymbols();
bool BindResourceApi(const PeImage&, void* module, ResourceApi& out, std::string& error);
struct ResourceClock {
    uint64_t (*now)() = nullptr;
    void (*pause)() = nullptr;
};
class NativeResources {
  public:
    explicit NativeResources(const ResourceApi& api, ResourceClock clock = {});
    // Startup must supply the private ResourceLocal.dat archive, never Gw2.dat.
    bool Seed(const Resources&, void* privateArchive, std::string& error);
    bool ProbeAndPublish(const Resources&, void* mainArchive, bool publish, std::string& error);
    bool Seeded() const {
        return seeded_;
    }
    bool Published() const {
        return published_;
    }

  private:
    bool Ready() const;
    void Wait(void* operation, uint64_t deadline) const;
    bool Revisions(const Resources&, void* archive, std::string& error) const;
    ResourceApi api_;
    ResourceClock clock_;
    bool seeded_ = false, published_ = false;
};
} // namespace july14812
} // namespace glide
