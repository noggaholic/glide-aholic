#include "file_hooks.h"
#include "network_policy.h"
#include "glide/log.h"
#include <Windows.h>
#include <winternl.h>
#include <cstdio>
#include <cstdarg>
#include <cwchar>

namespace glide {
namespace {
bool china_client = false;
Log* clientLog = nullptr;
wchar_t probe_nt[MAX_PATH + 4], probe_final[MAX_PATH + 4];
wchar_t archive_probe_nt[MAX_PATH + 4], archive_probe_final[MAX_PATH + 4];
wchar_t china_source_lock_nt[MAX_PATH + 32], china_private_lock_nt[MAX_PATH + 32];
wchar_t china_source_header_nt[MAX_PATH + 32], china_private_header_nt[MAX_PATH + 32];
wchar_t china_source_chunks_nt[MAX_PATH + 32];
LONG china_chunk_opens = 0;
void report(const char* format, ...) {
    char text[2048];
    va_list args;
    va_start(args, format);
    _vsnprintf_s(text, sizeof(text), _TRUNCATE, format, args);
    va_end(args);
    OutputDebugStringA(text);
    OutputDebugStringA("\n");
    if (clientLog)
        clientLog->Write("files", text);
}
static NTSTATUS(NTAPI* real_create_file)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER,
                                         ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
static NTSTATUS(NTAPI* real_write_file)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG,
                                        PLARGE_INTEGER, PULONG);
static NTSTATUS(NTAPI* real_write_gather)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK,
                                          PFILE_SEGMENT_ELEMENT, ULONG, PLARGE_INTEGER, PULONG);
static NTSTATUS(NTAPI* real_set_file)(HANDLE, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS);
static NTSTATUS(NTAPI* real_create_section)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG,
                                            HANDLE);
static NTSTATUS(NTAPI* real_fs_control)(HANDLE, HANDLE, PIO_APC_ROUTINE, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID, ULONG,
                                        PVOID, ULONG);
static NTSTATUS file_violation(const char* operation) {
    report("DENY updater %s", operation);
    TerminateProcess(GetCurrentProcess(), ERROR_ACCESS_DISABLED_BY_POLICY);
    return (NTSTATUS)0xc0000022L;
}
/* Query the handle, not a cached handle number: duplicates and handle reuse must
 * not escape protection. Only ordinary, empty, single-link QA probe files pass. */
static int updater_handle(HANDLE file) {
    wchar_t path[1024];
    DWORD count;
    if (GetFileType(file) != FILE_TYPE_DISK)
        return 0;
    count = GetFinalPathNameByHandleW(file, path, 1024, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!count || count >= 1024) {
        file_violation("unclassifiable disk handle");
        return 1;
    }
    return UpdaterTemporaryPath(path, count);
}
static int empty_probe_handle(HANDLE file) {
    wchar_t path[MAX_PATH + 4];
    BY_HANDLE_FILE_INFORMATION info;
    DWORD count = GetFinalPathNameByHandleW(file, path, MAX_PATH + 4, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (!count || count >= MAX_PATH + 4 || (_wcsicmp(path, probe_final) && _wcsicmp(path, archive_probe_final)))
        return 0;
    if (!GetFileInformationByHandle(file, &info)) {
        /* SetFileAttributes opens with WRITE_ATTRIBUTES only. Reopen the SAME
         * file for metadata (not by path); do not invent size or identity. */
        HANDLE metadata;
        BOOL ok;
        if (GetLastError() != ERROR_ACCESS_DENIED)
            return 0;
        metadata = ReOpenFile(file, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
        if (metadata == INVALID_HANDLE_VALUE)
            return 0;
        ok = GetFileInformationByHandle(metadata, &info);
        CloseHandle(metadata);
        if (!ok)
            return 0;
    }
    return info.nNumberOfLinks == 1 &&
           !(info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) &&
           !info.nFileSizeHigh && !info.nFileSizeLow;
}
static int china_probe_child(const OBJECT_ATTRIBUTES* oa) {
    size_t count, prefix, china_prefix;
    wchar_t root_path[MAX_PATH + 4];
    DWORD length;
    if (!china_client || !oa || !oa->ObjectName || !oa->ObjectName->Buffer)
        return 0;
    count = oa->ObjectName->Length / sizeof(wchar_t);
    prefix = wcslen(probe_nt);
    china_prefix = wcslen(archive_probe_nt);
    if (count > prefix && !_wcsnicmp(oa->ObjectName->Buffer, probe_nt, prefix) &&
        (oa->ObjectName->Buffer[prefix] == L'\\' || oa->ObjectName->Buffer[prefix] == L'/'))
        return 1;
    if (count > china_prefix && !_wcsnicmp(oa->ObjectName->Buffer, archive_probe_nt, china_prefix) &&
        (oa->ObjectName->Buffer[china_prefix] == L'\\' || oa->ObjectName->Buffer[china_prefix] == L'/'))
        return 1;
    if (!oa->RootDirectory)
        return 0;
    length =
        GetFinalPathNameByHandleW(oa->RootDirectory, root_path, MAX_PATH + 4, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length && length < MAX_PATH + 4 &&
        (!_wcsicmp(root_path, probe_final) || !_wcsicmp(root_path, archive_probe_final)) && count)
        return 1;
    if (count > 11 && !_wcsnicmp(oa->ObjectName->Buffer, L"Gw2-64.tmp", 10) &&
        (oa->ObjectName->Buffer[10] == L'\\' || oa->ObjectName->Buffer[10] == L'/'))
        return 1;
    if (count > 8 && !_wcsnicmp(oa->ObjectName->Buffer, L"Gw2.tmp", 7) &&
        (oa->ObjectName->Buffer[7] == L'\\' || oa->ObjectName->Buffer[7] == L'/'))
        return 1;
    return 0;
}
static int china_source_chunk(const OBJECT_ATTRIBUTES* oa) {
    size_t count, prefix;
    if (!china_client || !china_source_chunks_nt[0] || !oa || oa->RootDirectory || !oa->ObjectName ||
        !oa->ObjectName->Buffer)
        return 0;
    count = oa->ObjectName->Length / sizeof(wchar_t);
    prefix = wcslen(china_source_chunks_nt);
    return count > prefix + 4 && !_wcsnicmp(oa->ObjectName->Buffer, china_source_chunks_nt, prefix) &&
           !_wcsnicmp(oa->ObjectName->Buffer + count - 4, L".dat", 4);
}
static NTSTATUS NTAPI guard_create_file(PHANDLE h, ACCESS_MASK access, POBJECT_ATTRIBUTES oa, PIO_STATUS_BLOCK io,
                                        PLARGE_INTEGER size, ULONG attr, ULONG share, ULONG disp, ULONG opts, PVOID ea,
                                        ULONG easz) {
    if (china_client && china_source_lock_nt[0] && oa && !oa->RootDirectory && oa->ObjectName) {
        const wchar_t* private_path = NULL;
        if (oa->ObjectName->Length == wcslen(china_source_lock_nt) * sizeof(wchar_t) &&
            !_wcsnicmp(oa->ObjectName->Buffer, china_source_lock_nt, wcslen(china_source_lock_nt)))
            private_path = china_private_lock_nt;
        else if (oa->ObjectName->Length == wcslen(china_source_header_nt) * sizeof(wchar_t) &&
                 !_wcsnicmp(oa->ObjectName->Buffer, china_source_header_nt, wcslen(china_source_header_nt)))
            private_path = china_private_header_nt;
        if (private_path) {
            UNICODE_STRING private_name;
            OBJECT_ATTRIBUTES private_attributes = *oa;
            private_name.Buffer = (PWSTR)private_path;
            private_name.Length = (USHORT)(wcslen(private_path) * sizeof(wchar_t));
            private_name.MaximumLength = private_name.Length + sizeof(wchar_t);
            private_attributes.ObjectName = &private_name;
            report("China main archive %s redirected to private client cache",
                   private_path == china_private_lock_nt ? "Lock.dat" : "header 0.dat");
            return real_create_file(h, access, &private_attributes, io, size, attr, share, disp, opts, ea, easz);
        }
    }
    if (china_source_chunk(oa)) {
        ACCESS_MASK read_access = (access & ~(GENERIC_WRITE | GENERIC_ALL | DELETE | FILE_WRITE_DATA |
                                              FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES | FILE_WRITE_EA)) |
                                  GENERIC_READ;
        // The native archive also probes existing chunks with exclusive access.
        // Keep the launcher's independent read locks compatible with those reads.
        NTSTATUS status =
            real_create_file(h, read_access, oa, io, NULL, attr, share | FILE_SHARE_READ, 1, opts, NULL, 0);
        LONG opened = InterlockedIncrement(&china_chunk_opens);
        if (opened <= 100 || opened % 10000 == 0)
            report("China main archive chunk read-only opens=%ld requested=%08lx result=%08lx path=%.*ls", opened,
                   access, status, oa->ObjectName->Length / 2, oa->ObjectName->Buffer);
        return status;
    }
    if (china_probe_child(oa) && (disp != 1 || (access & (GENERIC_WRITE | GENERIC_ALL | DELETE | FILE_WRITE_DATA |
                                                          FILE_APPEND_DATA | FILE_WRITE_ATTRIBUTES))))
        return file_violation("China updater child create/write");
    if (oa && oa->ObjectName && UpdaterTemporaryPath(oa->ObjectName->Buffer, oa->ObjectName->Length / sizeof(wchar_t)))
        report("updater probe open path=%.*ls root=%p access=%08lx disposition=%lu options=%08lx allocation=%lld",
               oa->ObjectName->Length / 2, oa->ObjectName->Buffer, oa->RootDirectory, access, disp, opts,
               size ? size->QuadPart : 0);
    if (oa && oa->ObjectName &&
        UpdaterTemporaryPath(oa->ObjectName->Buffer, oa->ObjectName->Length / sizeof(wchar_t)) &&
        (disp != 1 || (access & (GENERIC_WRITE | GENERIC_ALL | DELETE | FILE_WRITE_DATA | FILE_APPEND_DATA |
                                 FILE_WRITE_ATTRIBUTES)))) {
        NTSTATUS status;
        /* OPEN/CREATE/OPEN_IF do not truncate existing evidence. No aliases,
         * other directories, streams, nonzero allocation or overwrite allowed. */
        int exact_64 = oa->ObjectName->Length == wcslen(probe_nt) * sizeof(wchar_t) &&
                       !_wcsnicmp(oa->ObjectName->Buffer, probe_nt, wcslen(probe_nt));
        // Build 204489 also probes Gw2.tmp when the private Gw2.dat does not exist yet.
        // Both names must obey the same empty, regular, single-link file policy.
        int exact_archive = oa->ObjectName->Length == wcslen(archive_probe_nt) * sizeof(wchar_t) &&
                            !_wcsnicmp(oa->ObjectName->Buffer, archive_probe_nt, wcslen(archive_probe_nt));
        if (oa->RootDirectory || (!exact_64 && !exact_archive) || (disp != 1 && disp != 2 && disp != 3) ||
            (size && size->QuadPart) || easz) {
            report(
                "probe rejected path=%.*ls root=%p access=%08lx disposition=%lu options=%08lx allocation=%lld ea=%lu",
                oa->ObjectName->Length / 2, oa->ObjectName->Buffer, oa->RootDirectory, access, disp, opts,
                size ? size->QuadPart : 0, easz);
            return file_violation("create outside empty QA probe policy");
        }
        status = real_create_file(h, access, oa, io, size, attr, share, disp, opts, ea, easz);
        if (status >= 0 && china_client && (exact_64 || exact_archive) && disp == 2 && (opts & 1)) {
            /* FILE_CREATE + FILE_DIRECTORY_FILE returned success for the exact
             * QA path: this is a newly created directory, never an alias or
             * pre-existing updater payload. The handle lacks READ_ATTRIBUTES. */
            report("ALLOW China newly created QA updater directory");
            return status;
        }
        if (status >= 0 && !empty_probe_handle(*h))
            return file_violation("nonempty or aliased QA probe");
        if (status >= 0)
            report("ALLOW empty QA permission probe");
        return status;
    }
    {
        NTSTATUS status = real_create_file(h, access, oa, io, size, attr, share, disp, opts, ea, easz);
        if (oa && oa->ObjectName && oa->ObjectName->Length >= 8 &&
            !_wcsnicmp(oa->ObjectName->Buffer + oa->ObjectName->Length / 2 - 4, L".dat", 4))
            report("archive open path=%.*ls access=%08lx disposition=%lu status=%08lx", oa->ObjectName->Length / 2,
                   oa->ObjectName->Buffer, access, disp, status);
        return status;
    }
}
static int empty_write(HANDLE h, ULONG length, PLARGE_INTEGER offset) {
    LARGE_INTEGER zero = {0}, position;
    return !length && empty_probe_handle(h) && (!offset || offset->QuadPart == 0 || offset->QuadPart == -2) &&
           SetFilePointerEx(h, zero, &position, FILE_CURRENT) && !position.QuadPart;
}
static NTSTATUS NTAPI guard_write_file(HANDLE h, HANDLE event, PIO_APC_ROUTINE apc, PVOID context, PIO_STATUS_BLOCK io,
                                       PVOID buffer, ULONG length, PLARGE_INTEGER offset, PULONG key) {
    if (updater_handle(h) && !empty_write(h, length, offset))
        return file_violation("data write");
    return real_write_file(h, event, apc, context, io, buffer, length, offset, key);
}
static NTSTATUS NTAPI guard_write_gather(HANDLE h, HANDLE event, PIO_APC_ROUTINE apc, PVOID context,
                                         PIO_STATUS_BLOCK io, PFILE_SEGMENT_ELEMENT segments, ULONG length,
                                         PLARGE_INTEGER offset, PULONG key) {
    if (updater_handle(h))
        return file_violation("gather write");
    return real_write_gather(h, event, apc, context, io, segments, length, offset, key);
}
static NTSTATUS NTAPI guard_set_file(HANDLE h, PIO_STATUS_BLOCK io, PVOID data, ULONG size,
                                     FILE_INFORMATION_CLASS type) {
    if (updater_handle(h)) {
        /* Native probe uses FileEndOfFileInformation=20. Position=14 and
         * Allocation=19 may only stay at zero; attributes/timestamps (4) and
         * deletion do not add data. Startup clears old probe attributes first. */
        int zero_size = (type == 14 || type == 19 || type == 20) && data && size >= sizeof(LARGE_INTEGER) &&
                        !((LARGE_INTEGER*)data)->QuadPart;
        if (!empty_probe_handle(h) || (!zero_size && type != 4 && type != 13 && type != 64)) {
            report("probe set-information rejected class=%u bytes=%lu first=%lld", (unsigned)type, size,
                   data && size >= sizeof(LARGE_INTEGER) ? ((LARGE_INTEGER*)data)->QuadPart : 0);
            return file_violation("resize/metadata/rename");
        }
    }
    return real_set_file(h, io, data, size, type);
}
static NTSTATUS NTAPI guard_create_section(PHANDLE section, ACCESS_MASK access, POBJECT_ATTRIBUTES oa,
                                           PLARGE_INTEGER size, ULONG protect, ULONG attr, HANDLE file) {
    if (file && updater_handle(file))
        return file_violation("file mapping");
    return real_create_section(section, access, oa, size, protect, attr, file);
}
static NTSTATUS NTAPI guard_fs_control(HANDLE file, HANDLE event, PIO_APC_ROUTINE apc, PVOID context,
                                       PIO_STATUS_BLOCK io, ULONG code, PVOID in, ULONG insz, PVOID out, ULONG outsz) {
    if (updater_handle(file))
        return file_violation("filesystem control");
    return real_fs_control(file, event, apc, context, io, code, in, insz, out, outsz);
}

} // namespace
bool InstallFileHooks(Hooks& hooks, const FileProtection& paths, std::string& error) {
    clientLog = paths.log;
    error = "Cannot prepare file protection";
    if (real_create_file)
        return false;
    const auto normalize = [](const std::wstring& input, std::wstring& out) {
        if (input.size() < 3 || input[1] != L':' || (input[2] != L'\\' && input[2] != L'/') ||
            input.find(L'\0') != std::wstring::npos)
            return false;
        wchar_t full[MAX_PATH];
        const DWORD count = GetFullPathNameW(input.c_str(), MAX_PATH, full, nullptr);
        if (!count || count >= MAX_PATH - 40)
            return false;
        out = full;
        while (out.size() > 3 && out.back() == L'\\')
            out.pop_back();
        return true;
    };
    std::wstring runtime, archive;
    if (!normalize(paths.runtimeDirectory, runtime) || (paths.china && !normalize(paths.sourceArchive, archive)))
        return false;
    china_client = paths.china;
    china_chunk_opens = 0;
    const auto name = [&](const wchar_t* prefix, const wchar_t* file) {
        return std::wstring(prefix) + runtime + L"\\" + file;
    };
    wcscpy_s(probe_nt, name(L"\\??\\", L"Gw2-64.tmp").c_str());
    wcscpy_s(probe_final, name(L"\\\\?\\", L"Gw2-64.tmp").c_str());
    wcscpy_s(archive_probe_nt, name(L"\\??\\", L"Gw2.tmp").c_str());
    wcscpy_s(archive_probe_final, name(L"\\\\?\\", L"Gw2.tmp").c_str());
    china_source_lock_nt[0] = china_source_header_nt[0] = china_source_chunks_nt[0] = 0;
    if (paths.china) {
        wcscpy_s(china_source_lock_nt, (L"\\??\\" + archive + L"\\Lock.dat").c_str());
        wcscpy_s(china_source_header_nt, (L"\\??\\" + archive + L"\\0-1-100\\0.dat").c_str());
        wcscpy_s(china_source_chunks_nt, (L"\\??\\" + archive + L"\\").c_str());
        wcscpy_s(china_private_lock_nt, name(L"\\??\\", L"MainLock.dat").c_str());
        wcscpy_s(china_private_header_nt, name(L"\\??\\", L"MainHeader0.dat").c_str());
    }
    struct Export {
        const char* name;
        void* replacement;
        void** original;
    };
    const Export exports[] = {
        {"NtCreateFile", reinterpret_cast<void*>(guard_create_file), reinterpret_cast<void**>(&real_create_file)},
        {"NtWriteFile", reinterpret_cast<void*>(guard_write_file), reinterpret_cast<void**>(&real_write_file)},
        {"NtWriteFileGather", reinterpret_cast<void*>(guard_write_gather),
         reinterpret_cast<void**>(&real_write_gather)},
        {"NtSetInformationFile", reinterpret_cast<void*>(guard_set_file), reinterpret_cast<void**>(&real_set_file)},
        {"NtCreateSection", reinterpret_cast<void*>(guard_create_section),
         reinterpret_cast<void**>(&real_create_section)},
        {"NtFsControlFile", reinterpret_cast<void*>(guard_fs_control), reinterpret_cast<void**>(&real_fs_control)}};
    for (const auto& entry : exports)
        if (!hooks.AttachExport(L"ntdll.dll", entry.name, entry.replacement, entry.original, error))
            return false;
    error.clear();
    return true;
}
} // namespace glide
