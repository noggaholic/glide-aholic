#pragma once
#include "glide/pe.h"
#include <Windows.h>
#include <string>
namespace glide {
// Owns the helper process tree; caller retains ownership of the suspended game.
// Any failure terminates the target before returning.
bool InjectVia32(HANDLE target, const std::wstring& helper, const std::wstring& dll, DWORD timeoutMs,
                 std::string& error);
bool InjectClient(HANDLE target, Bitness bitness, const std::wstring& binaries, DWORD timeoutMs, std::string& error);
} // namespace glide
