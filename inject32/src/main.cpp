#include "glide/injection.h"
#include <cstdio>
#include <cwchar>

namespace {
bool Number(const wchar_t* text, DWORD maximum, DWORD& out) {
    if (!text || !*text)
        return false;
    DWORD value = 0;
    for (; *text; ++text) {
        if (*text < L'0' || *text > L'9' || value > (maximum - static_cast<DWORD>(*text - L'0')) / 10)
            return false;
        value = value * 10 + static_cast<DWORD>(*text - L'0');
    }
    if (!value)
        return false;
    out = value;
    return true;
}
} // namespace
int wmain(int argc, wchar_t** argv) {
    static_assert(sizeof(void*) == 4, "Build inject32 for Win32");
    DWORD pid = 0, timeout = 30000;
    if ((argc != 3 && argc != 4) || !Number(argv[1], MAXDWORD, pid) ||
        (argc == 4 && !Number(argv[3], 300000, timeout))) {
        std::fputs("Usage: inject32 <pid> <absolute-dll> [timeout-ms]\n", stderr);
        return 2;
    }
    HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION |
                                     PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_TERMINATE | SYNCHRONIZE,
                                 FALSE, pid);
    if (!process) {
        std::fputs("Cannot open the suspended client\n", stderr);
        return 3;
    }
    std::string error;
    const bool ready = glide::InjectAndWait(process, argv[2], timeout, error);
    CloseHandle(process);
    if (!ready)
        std::fprintf(stderr, "inject32: %s\n", error.c_str());
    return ready ? 0 : 4;
}
