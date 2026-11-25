#include "game.h"
#include "../../14505/server/game.h"

namespace glide {
namespace june14422 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent) {
    return june14505::MakeGameMessagesForBuild(14422, ownedAgent);
}
const GameMessages& ServiceMessages() {
    return *june14505::ServiceMessagesForBuild(14422);
}
} // namespace june14422
} // namespace glide
