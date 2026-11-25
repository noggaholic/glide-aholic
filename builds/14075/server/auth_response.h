#pragma once
#include "../../14505/server/auth_response.h"

namespace glide {
namespace april14075 {
using RosterCharacter = june14505::RosterCharacter;
bool AccountReply(const AccountState& account, const Guid& selected, uint32_t request, ByteView contentSeed,
                  Buffer& out);
bool CharacterEntry(const RosterCharacter& character, uint32_t request, Buffer& out);
bool LoginComplete(const std::vector<RosterCharacter>& characters, uint32_t request, std::vector<Buffer>& out,
                   ByteView config = ByteView());
bool SelectionReply(uint32_t request, uint32_t map, Buffer& out);
} // namespace april14075
} // namespace glide
