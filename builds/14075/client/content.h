#pragma once
#include "glide/bxml_documents.h"

namespace glide {
namespace april14075 {
class Content {
  public:
    bool Load(const std::wstring& directory, std::string& error);
    bool Prepare(std::string& error);
    ByteView Document(const wchar_t* name) const {
        return documents_.Document(name);
    }
    size_t Count() const {
        return documents_.Count();
    }
    static ByteView Key();

  private:
    BxmlDocuments documents_;
};
} // namespace april14075
} // namespace glide
