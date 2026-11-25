#include "content.h"
#include "glide/profile/june_content_landing.h"
namespace glide {
bool JuneContent::Load(const std::wstring& directory, std::string& error) {
    BxmlDocuments candidate;
    if (!candidate.Load(directory, 14505, error) || !beta::ApplyWaterLanding(14505, candidate, error))
        return false;
    documents_ = std::move(candidate);
    return true;
}
bool JuneContent::Prepare(ByteView seed, std::string& error) {
    return documents_.Prepare(seed, error);
}
ByteView JuneContent::Document(const wchar_t* name) const {
    return documents_.Document(name);
}
} // namespace glide
