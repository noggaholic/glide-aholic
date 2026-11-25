#pragma once
#include "hooks.h"
#include "glide/slot_config.h"

namespace glide {
class Log;
struct ChildProtection {
    Log* log = nullptr;
    std::wstring dllPath;
    std::vector<std::wstring> executables;
    unsigned long readyTimeoutMs = 15000;
    SlotConfig slot;
};
bool InstallChildHooks(Hooks& hooks, const ChildProtection& protection, std::string& error);
} // namespace glide
