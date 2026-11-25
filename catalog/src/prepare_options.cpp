#include "prepare_options.h"
#include "glide/files.h"
#include "glide/kv.h"
#include <cwchar>
#include <cstdio>
int ParsePrepareOptions(int argc, wchar_t** argv, PrepareOptions& options) {
    auto& requested = options.requested;
    auto& output = options.output;
    auto& outSet = options.outSet;
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 == argc) {
            std::puts("Missing option value");
            return 2;
        }
        std::string number;
        if (!std::wcscmp(argv[i], L"--build") && !requested && glide::WideToUtf8(argv[i + 1], number) &&
            glide::ParseUnsigned(number, UINT32_MAX, requested) && requested)
            continue;
        if (!std::wcscmp(argv[i], L"--out") && !outSet) {
            output = argv[i + 1];
            outSet = true;
            continue;
        }
        std::puts("Unknown, repeated or invalid option");
        return 2;
    }
    return 0;
}
