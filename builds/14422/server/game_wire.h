#pragma once
#include "glide/profile/wire_map.h"
#include <memory>

namespace glide {
namespace june14422 {
bool CreateGameWire(const std::string& login, std::unique_ptr<WireMap>& out, std::string& error);
}
} // namespace glide
