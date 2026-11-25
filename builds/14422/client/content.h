#pragma once
#include "glide/bxml_documents.h"

namespace glide {
namespace june14422 {
class Content {
  public:
    bool Load(const std::wstring& directory, std::string& error);
    // Preserve the original June seed range (1..1024 bytes), not July's exact-32 rule.
    bool Prepare(ByteView seed, std::string& error);
    ByteView Document(const wchar_t* name) const {
        return documents_.Document(name);
    }
    size_t Count() const {
        return documents_.Count();
    }

  private:
    BxmlDocuments documents_;
};
} // namespace june14422
} // namespace glide
