#pragma once
#include "glide/profile/create_flow.h"
#include <memory>

namespace glide {
namespace june14422 {
bool CreateCharacterFlow(const std::wstring& dataRoot, const SpawnPoint& initial, std::unique_ptr<CreateFlow>& out,
                         std::string& error);
}
} // namespace glide
