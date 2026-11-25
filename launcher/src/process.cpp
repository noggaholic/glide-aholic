// Creation-time job assignment requires Windows 10. Keep other components' SDK target unchanged.
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#include "process.h"

namespace glide {
std::wstring QuoteArgument(const std::wstring& argument) {
    std::wstring out = L"\"";
    size_t slashes = 0;
    for (wchar_t c : argument) {
        if (c == L'\\') {
            ++slashes;
            continue;
        }
        out.append(c == L'\"' ? slashes * 2 + 1 : slashes, L'\\');
        out += c;
        slashes = 0;
    }
    out.append(slashes * 2, L'\\');
    out += L'\"';
    return out;
}
ProcessTree::~ProcessTree() {
    Stop();
    if (thread_)
        CloseHandle(thread_);
    if (process_)
        CloseHandle(process_);
    if (job_)
        CloseHandle(job_);
}
bool ProcessTree::StartSuspended(const std::wstring& executable, const std::vector<std::wstring>& arguments,
                                 const std::wstring& directory, const std::vector<wchar_t>& environment,
                                 std::string& error, const std::wstring& jobName) {
    error = "Cannot start suspended process";
    if (job_ || executable.empty() || executable.find(L'\0') != std::wstring::npos ||
        directory.find(L'\0') != std::wstring::npos ||
        (!environment.empty() && (environment.size() < 2 || environment.back() || environment[environment.size() - 2])))
        return false;
    std::wstring command = QuoteArgument(executable);
    for (const auto& argument : arguments) {
        if (argument.find(L'\0') != std::wstring::npos)
            return false;
        command += L" " + QuoteArgument(argument);
    }
    if (command.size() >= 32767)
        return false;
    if (jobName.find(L'\0') != std::wstring::npos)
        return false;
    HANDLE job = CreateJobObjectW(nullptr, jobName.empty() ? nullptr : jobName.c_str());
    const DWORD jobError = GetLastError();
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits = {};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!job)
        return false;
    if (!jobName.empty() && jobError == ERROR_ALREADY_EXISTS) {
        error = "Private runtime process group already exists";
        CloseHandle(job);
        return false;
    }
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits))) {
        CloseHandle(job);
        return false;
    }
    SIZE_T attributeSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
    std::vector<unsigned char> attributes(attributeSize);
    STARTUPINFOEXW startup = {};
    startup.StartupInfo.cb = sizeof(startup);
    startup.lpAttributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributes.data());
    if (!attributeSize || !InitializeProcThreadAttributeList(startup.lpAttributeList, 1, 0, &attributeSize)) {
        CloseHandle(job);
        return false;
    }
    if (!UpdateProcThreadAttribute(startup.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &job, sizeof(job),
                                   nullptr, nullptr)) {
        error = "Cannot assign process group during creation (Windows 10 or newer required)";
        DeleteProcThreadAttributeList(startup.lpAttributeList);
        CloseHandle(job);
        return false;
    }
    PROCESS_INFORMATION process = {};
    std::vector<wchar_t> localEnvironment = environment;
    const BOOL created =
        CreateProcessW(executable.c_str(), &command[0], nullptr, nullptr, FALSE,
                       CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
                       localEnvironment.empty() ? nullptr : localEnvironment.data(),
                       directory.empty() ? nullptr : directory.c_str(), &startup.StartupInfo, &process);
    const DWORD createError = GetLastError();
    DeleteProcThreadAttributeList(startup.lpAttributeList);
    if (!created) {
        error += " (Windows " + std::to_string(createError) + ")";
        CloseHandle(job);
        return false;
    }
    job_ = job;
    process_ = process.hProcess;
    thread_ = process.hThread;
    pid_ = process.dwProcessId;
    error.clear();
    return true;
}
bool ProcessTree::Resume(std::string& error) {
    error = "Cannot resume process";
    if (!thread_ || resumed_ || WaitForSingleObject(process_, 0) != WAIT_TIMEOUT ||
        ResumeThread(thread_) == static_cast<DWORD>(-1))
        return false;
    resumed_ = true;
    error.clear();
    return true;
}
bool ProcessTree::Active(unsigned& count) const {
    if (!job_)
        return false;
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = {};
    if (!QueryInformationJobObject(job_, JobObjectBasicAccountingInformation, &info, sizeof(info), nullptr))
        return false;
    count = info.ActiveProcesses;
    return true;
}
bool ProcessTree::Stop(DWORD timeoutMs) {
    if (!job_)
        return true;
    unsigned count = 0;
    if (!Active(count))
        return false;
    if (!count)
        return true;
    if (!TerminateJobObject(job_, ERROR_CANCELLED))
        return false;
    const ULONGLONG start = GetTickCount64();
    do {
        if (!Active(count))
            return false;
        if (!count)
            return true;
        if (GetTickCount64() - start >= timeoutMs)
            return false;
        Sleep(10);
    } while (true);
}
} // namespace glide
