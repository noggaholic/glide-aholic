#include "glide/accounts.h"
#include "glide/builds.h"
#include <cstdio>
#include <cwchar>
#include <iostream>
#include <string>

namespace {
void Usage() {
    std::puts("Usage: glide-accounts [--build N] [--email E] [--password P] [--root state-dir] [--list] [--force]");
    std::puts("Missing build/email/password are requested interactively. --force only updates an existing password.");
}
bool Prompt(const char* label, std::string& text) {
    std::cout << label << ": " << std::flush;
    return static_cast<bool>(std::getline(std::cin, text));
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    std::wstring root = L"state";
    std::string buildText, email, password;
    bool list = false, force = false;
    unsigned seen = 0;
    for (int i = 1; i < argc; ++i) {
        const std::wstring arg(argv[i]);
        if (arg == L"--help") {
            Usage();
            return 0;
        }
        const unsigned bit = arg == L"--build"      ? 1
                             : arg == L"--email"    ? 2
                             : arg == L"--password" ? 4
                             : arg == L"--root"     ? 8
                             : arg == L"--list"     ? 16
                             : arg == L"--force"    ? 32
                                                    : 0;
        if (!bit || (seen & bit)) {
            Usage();
            return 2;
        }
        seen |= bit;
        if (bit == 16) {
            list = true;
            continue;
        }
        if (bit == 32) {
            force = true;
            continue;
        }
        if (++i == argc) {
            Usage();
            return 2;
        }
        if (bit == 8) {
            root = argv[i];
            continue;
        }
        std::string value;
        if (!glide::WideToUtf8(argv[i], value)) {
            std::puts("Invalid Unicode argument");
            return 2;
        }
        if (bit == 1)
            buildText = value;
        else if (bit == 2)
            email = value;
        else
            password = value;
    }
    if (list && (seen & (2 | 4 | 32))) {
        Usage();
        return 2;
    }
    if (buildText.empty() && !Prompt("Build", buildText))
        return 2;
    unsigned build = 0;
    if (!glide::ParseUnsigned(buildText, UINT32_MAX, build) || !glide::BuildTable::Find(build)) {
        std::puts("Unsupported build. Available: 204489 14505 49284 14812 14422 14075 167409");
        return 2;
    }
    if (!list) {
        if (email.empty() && !Prompt("Email", email))
            return 2;
        if (password.empty() && !Prompt("Password", password))
            return 2;
    }
    glide::AccountFiles accounts;
    std::string error;
    if (!accounts.Open(root, build, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    if (list) {
        std::vector<std::string> emails;
        if (!accounts.List(emails, error)) {
            std::fprintf(stderr, "%s\n", error.c_str());
            return 1;
        }
        for (const auto& entry : emails)
            std::puts(entry.c_str());
        std::printf("%zu account(s)\n", emails.size());
        return 0;
    }
    if (!accounts.Create(email, password, force, error)) {
        std::fprintf(stderr, "%s\n", error.c_str());
        return 1;
    }
    std::puts("Account saved.");
    return 0;
}
