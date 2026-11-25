#pragma once
#include "character_catalog.h"
namespace glide {
namespace july14812 {
struct CreateRequest {
    std::string name;
    std::array<uint8_t, 48> appearance{};
    uint8_t profession = 0, level = 0;
    uint32_t flags = 0;
    std::vector<uint8_t> biography;
    std::array<std::array<uint32_t, 4>, 6> colors{};
};
struct CreatedCharacter {
    std::array<uint8_t, 16> character{}, account{};
    std::string name;
    uint16_t retainedWord = 0;
    CharacterSummary summary;
};
// Structural formats are separate from recipe/identity/admission checks.
DecodeStatus DecodeCreateRequest(ByteView, CreateRequest&, size_t& consumed);
bool EncodeCreateRequest(const CreateRequest&, Buffer&);
bool DecodeCreated(ByteView, CreatedCharacter&);
bool EncodeCreated(const CreatedCharacter&, Buffer&);
DecodeStatus DecodeSubmission(ByteView, const CharacterCatalog&, Character&, size_t& consumed);
bool CreatedReply(const Guid& account, const Character&, const CharacterCatalog&, Buffer&);
} // namespace july14812
} // namespace glide
