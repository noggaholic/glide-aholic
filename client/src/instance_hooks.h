#pragma once
#include "hooks.h"

namespace glide {
// Opt-in from profiles whose original client uses the AN-Mutex- instance family.
bool InstallInstanceHooks(Hooks& hooks, unsigned build, unsigned slot, std::string& error);
} // namespace glide
