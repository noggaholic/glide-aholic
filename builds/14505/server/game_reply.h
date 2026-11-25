#pragma once
#include "glide/buffer.h"
#include "glide/guid.h"
#include <string>

namespace glide {
namespace june14505 {
bool ChatReply(const std::string& text, Buffer& out);
bool ChatPublication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                     uint8_t display, Buffer& out);
bool MapTransfer(uint32_t association, unsigned map, Buffer& out);
bool ChatPublicationForBuild(unsigned build, const std::string& text, const std::string& sender, const Guid& identity,
                             uint8_t channel, uint8_t display, Buffer& out);
bool ChatReplyForBuild(unsigned build, const std::string& text, Buffer& out);
bool MapTransferForBuild(unsigned build, uint32_t association, unsigned map, Buffer& out);
} // namespace june14505
} // namespace glide
