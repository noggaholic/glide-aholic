#include "prepare.h"
#include "complete.h"
#include "glide/builds.h"
#include "glide/installation_data.h"
#include <cerrno>
#include <cstdio>
#include <cwchar>

int wmain(int argc, wchar_t** argv) {
    using namespace glide;
    if (argc > 1 && !std::wcscmp(argv[1], L"complete"))
        return CompleteMain(argc - 1, argv + 1);
    if (argc > 1 && !std::wcscmp(argv[1], L"prepare"))
        return PrepareMain(argc - 1, argv + 1);
    if (argc < 2) {
        std::puts("Usage: glide-catalog <Gw2.dat> [--build N] [--out directory]");
        return 2;
    }
    unsigned requested = 0;
    std::wstring output;
    for (int i = 2; i < argc; i += 2) {
        if (i + 1 == argc) {
            std::puts("Missing option value");
            return 2;
        }
        if (!std::wcscmp(argv[i], L"--out") && output.empty())
            output = argv[i + 1];
        else if (!std::wcscmp(argv[i], L"--build") && !requested) {
            wchar_t* end = nullptr;
            errno = 0;
            const auto value = std::wcstoul(argv[i + 1], &end, 10);
            if (errno || !value || argv[i + 1][0] == L'-' || !end || *end) {
                std::puts("Invalid build");
                return 2;
            }
            requested = value;
        } else {
            std::puts("Unknown or repeated option");
            return 2;
        }
    }
    InstallationData data;
    std::string error;
    if (!data.Open(argv[1], requested, error) || !BuildTable::Find(data.Build())) {
        std::puts("Missing or mismatched local installation data; complete this installation first");
        return 1;
    }
    if (output.empty())
        output = L"data/" + std::to_wstring(data.Build()) + L"/catalog";
    if (!data.Extract(output, error, "catalog/")) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::printf("Build %u: catalog extracted from the completed installation\n", data.Build());
    return 0;
}
