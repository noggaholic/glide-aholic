#pragma once
#include "glide/bxml_documents.h"

namespace glide {
class JuneContent {
  public:
    // Atomic load; failed replacement retains the current catalog and encryption.
    bool Load(const std::wstring& directory, std::string& error);
    bool Prepare(ByteView seed, std::string& error);
    // Null selects the manifest. Views are valid until successful Load or destruction;
    // Prepare changes bytes in place. Caller serializes loading and native parsing.
    ByteView Document(const wchar_t* name) const;
    size_t Count() const {
        return documents_.Count();
    }

  private:
    BxmlDocuments documents_;
};
} // namespace glide
