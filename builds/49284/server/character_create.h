#pragma once
#include "character_catalog.h"
namespace glide {
namespace hot49284 {
// Parses own C251 and constructs the shared saved representation from own recipes.
// Caller supplies durable identity/slot only after authorization and successful save.
DecodeStatus DecodeSubmission(ByteView, const CharacterCatalog&, Character&, size_t& consumed);
bool CreatedReply(const Guid& account, const Character&, const CharacterCatalog&, Buffer&);
} // namespace hot49284
} // namespace glide
