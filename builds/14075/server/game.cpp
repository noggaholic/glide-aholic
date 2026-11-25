#include "game.h"

namespace glide {
namespace april14075 {
std::unique_ptr<GameMessages> MakeGameMessages(uint16_t ownedAgent) {
    return june14505::MakeGameMessagesForBuild(14075, ownedAgent);
}
const GameMessages& ServiceMessages() {
    return *june14505::ServiceMessagesForBuild(14075);
}
} // namespace april14075
} // namespace glide
