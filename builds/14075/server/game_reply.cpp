#include "game_reply.h"

namespace glide {
namespace april14075 {
bool ChatReply(const std::string& text, Buffer& out) {
    return june14505::ChatReplyForBuild(14075, text, out);
}
bool ChatPublication(const std::string& text, const std::string& sender, const Guid& identity, uint8_t channel,
                     uint8_t display, Buffer& out) {
    return june14505::ChatPublicationForBuild(14075, text, sender, identity, channel, display, out);
}
bool MapTransfer(uint32_t association, unsigned map, Buffer& out) {
    return june14505::MapTransferForBuild(14075, association, map, out);
}
} // namespace april14075
} // namespace glide
