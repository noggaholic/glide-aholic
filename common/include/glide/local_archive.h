#pragma once
#include <string>
namespace glide {
bool CreateLocalArchive(const std::wstring& templateFile, const std::wstring& output, std::string& error);
}
