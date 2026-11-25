#include "game_wire.h"
#include "../../14505/server/game_wire.h"

namespace glide {
namespace june14422 {
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error) {
    return june14505::CreateGameWire(login, out, error);
}
} // namespace june14422
} // namespace glide
