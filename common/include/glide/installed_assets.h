#pragma once
#include "glide/dat_archive.h"
#include <memory>
#include <mutex>
#include <set>
#include <vector>
namespace glide {
class Log;
struct InstalledAssetPair {
    uint32_t reference, file;
};
// Same verified x86 IManifest interface; mappings are owned by the build caller.
class InstalledAssets {
  public:
    explicit InstalledAssets(unsigned build) : build_(build) {}
    bool Open(const std::wstring& path, std::string& error);
    bool Query(uint32_t id, uint32_t* size = nullptr);
    bool Resolve(uint32_t reference, uint32_t* file = nullptr);
    bool Variant(uint32_t reference, uint8_t mode, uint32_t* file = nullptr);
    void* NativeManifest();
    // Configure before registering the native provider; sink must outlive it.
    void TraceRequests(Log* sink) {
        trace_ = sink;
    }
    size_t PairCount() const {
        return pairs_.size();
    }

  private:
    struct Provider {
        void** table = nullptr;
        InstalledAssets* owner = nullptr;
    } provider_;
    static void __fastcall Destroy(Provider*, void*, unsigned);
    static int __fastcall NativeQuery(Provider*, void*, uint32_t, uint32_t*, uint32_t*);
    static int __fastcall NativeResolve(Provider*, void*, uint32_t, uint32_t*);
    static int __fastcall Contains(Provider*, void*, uint32_t);
    static void __fastcall Cache(Provider*, void*, uint32_t, unsigned);
    static int __fastcall NativeVariant(Provider*, void*, uint32_t, uint8_t, uint32_t*);
    unsigned build_;
    std::vector<InstalledAssetPair> pairs_;
    std::mutex mutex_;
    std::unique_ptr<DatArchive> archive_;
    void Trace(const char* operation, uint32_t id, uint32_t file, uint8_t mode, bool found);
    Log* trace_ = nullptr;
    std::mutex traceMutex_;
    std::set<uint64_t> traced_;
};
} // namespace glide
