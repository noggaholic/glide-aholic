#pragma once
#include "glide/buffer.h"
#include <array>
#include <string>
namespace glide {
namespace july14812 {
class Resources {
  public:
    // Seven original names cover nine map IDs; three home maps share one.
    static constexpr size_t kManifestCount = 7;
    // All files validate before replacement or native publication can start.
    bool Load(const std::wstring& directory, std::string& error);
    ByteView Root() const {
        return root_.View();
    }
    ByteView Manifest(size_t index) const {
        return index < kManifestCount ? manifests_[index].View() : ByteView();
    }
    uint32_t RecordCount(size_t index) const {
        return index < kManifestCount ? counts_[index] : 0;
    }
    uint32_t PrivateId(size_t index) const;
    static const wchar_t* FileName(size_t index);

  private:
    Buffer root_;
    std::array<Buffer, kManifestCount> manifests_;
    std::array<uint32_t, kManifestCount> counts_{};
};
} // namespace july14812
} // namespace glide
