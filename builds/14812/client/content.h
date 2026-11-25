#pragma once
#include "glide/bxml_documents.h"
namespace glide {
namespace july14812 {
class Content {
  public:
    // Exact own July document hashes; failure retains previous buffers/encryption.
    bool Load(const std::wstring& directory, std::string& error);
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
} // namespace july14812
} // namespace glide
