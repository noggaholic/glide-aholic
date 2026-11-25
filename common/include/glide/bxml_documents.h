#pragma once
#include "glide/buffer.h"
#include <array>
#include <functional>
#include <map>
#include <string>
namespace glide {
enum class BxmlDocumentNames { Hashed, Named };
// Shared document format and cipher; each caller supplies its own build identity.
class BxmlDocuments {
  public:
    bool Load(const std::wstring& directory, uint32_t build, std::string& error,
              BxmlDocumentNames names = BxmlDocumentNames::Hashed);
    bool Prepare(ByteView seed, std::string& error);
    // April's resource provider supplies a fixed RC4 key instead of deriving a seed.
    bool PrepareKey(ByteView key, std::string& error);
    ByteView Document(const wchar_t* name) const;
    size_t Count() const {
        return documents_.size();
    }
    // Available only before encryption. Visitor must not reload/encrypt this instance.
    bool VisitPlaintext(const std::function<bool(const std::wstring&, ByteView)>& visitor) const;
    // Replace an existing document before encryption; reject invalid replacements
    // without changing the catalog. Build-specific adapters authenticate inputs.
    bool ReplacePlaintext(const wchar_t* name, Buffer&& replacement, std::string& error);

  private:
    std::map<std::wstring, Buffer> documents_;
    std::array<uint8_t, 20> key_{};
    BxmlDocumentNames names_ = BxmlDocumentNames::Hashed;
    bool prepared_ = false;
};
} // namespace glide
