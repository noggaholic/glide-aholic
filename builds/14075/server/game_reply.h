#pragma once
#include "../../14505/server/game_reply.h"

namespace glide {
namespace april14075 {
bool ChatReply(const std::string& text, Buffer& out);
bool ChatPublication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                     uint8_t display, Buffer& out);
bool MapTransfer(uint32_t association, unsigned map, Buffer& out);
} // namespace april14075
} // namespace glide
