#pragma once
#include "hooks.h"

namespace glide {
class Log;
struct FileProtection {
    std::wstring runtimeDirectory;
    std::wstring sourceArchive;
    bool china = false;
    Log* log = nullptr;
};
bool InstallFileHooks(Hooks& hooks, const FileProtection& paths, std::string& error);
} // namespace glide
