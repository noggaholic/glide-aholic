#pragma once
#include "network_policy.h"
#include "hooks.h"
#include "glide/log.h"

namespace glide {
// Prepare all network hooks in the process's shared Hooks owner. Enable only after all guards are ready.
bool InstallNetworkHooks(Hooks& hooks, const NetworkPolicy& policy, std::string& error, Log* log = nullptr);
} // namespace glide
