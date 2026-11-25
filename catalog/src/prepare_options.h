#pragma once
#include <string>
struct PrepareOptions {
    unsigned requested = 0;
    std::wstring output = L"data";
    bool outSet = false;
};
int ParsePrepareOptions(int argc, wchar_t** argv, PrepareOptions& options);
