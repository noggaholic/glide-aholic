#pragma once
#include <Windows.h>
#include <string>

namespace glide {
std::wstring ClientReadyEvent(DWORD pid);
// Caller owns a suspended process of the same architecture. Failure terminates that process.
bool InjectAndWait(HANDLE process, const std::wstring& dllPath, DWORD timeoutMs, std::string& error);
} // namespace glide
