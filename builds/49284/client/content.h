#pragma once
#include "glide/buffer.h"
#include "installed.h"
#include <array>
#include <string>

namespace glide {
namespace hot49284 {
class Content {
  public:
    // Exact retained native metadata. Failure preserves all existing views.
    bool Load(const std::wstring& directory, std::string& error);
    ByteView Root() const {
        return documents_[0].View();
    }
    ByteView Manifest(size_t index) const {
        return index < 5 ? documents_[index + 1].View() : ByteView();
    }
    // Validate every manifest before calling either native publisher.
    // Callbacks must not reload this Content or throw; caller owns serialization.
    bool Publish(Glide49284PublishAssetFn asset, Glide49284PublishPropertyFn property, std::string& error) const;
    static const wchar_t* FileName(size_t index);

  private:
    std::array<Buffer, 6> documents_;
};
} // namespace hot49284
} // namespace glide
